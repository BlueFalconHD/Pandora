#include "hexdump.h"
#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <mach/error.h>

#include "proc_ro.h"

#include "kstructs.h"
#include "memdiff.h"
#include "proc_task.h"

// for mach o parsing stuff
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define FUNC_PROC_FIND kslide(0xFFFFFE0008DFEE7CULL)
#define FUNC_PROC_RELE kslide(0xFFFFFE0008DFF518ULL)
#define FUNC_PROC_TASK kslide(0xFFFFFE0008E02274ULL)
#define FUNC_RO_FOR_PROC kslide(0xFFFFFE0008E010C0ULL)

#define PROC_STRUCT_SIZE_PTR kslide(0xFFFFFE000C551C30ULL)
#define PROC_STRUCT_SIZE_DEFAULT 0x7A0ULL
#define PROC_STRUCT_SIZE_MAX 0x10000ULL

#define PROC_OFF_RO_PTR 0x18ULL
#define PROC_OFF_FLAGS 0x468ULL
#define TASK_OFF_RO_PTR 0x3E8ULL

typedef struct {
  uint64_t fileoff;
  uint64_t filesize;
  uint64_t vmaddr;
} segment_map;

static uint64_t fileoff_to_vmaddr(const segment_map *segs, size_t seg_count,
                                  uint64_t fileoff) {
  for (size_t i = 0; i < seg_count; i++) {
    uint64_t start = segs[i].fileoff;
    uint64_t end = start + segs[i].filesize;
    if (fileoff >= start && fileoff < end) {
      return segs[i].vmaddr + (fileoff - start);
    }
  }
  return 0;
}

const char *string_by_strx(int idx, memdiff_view *strtab) {
  if (idx < 0 || (size_t)idx >= strtab->size) {
    printf("Invalid string index: %d\n", idx);
    return NULL;
  }

  char *str = (char *)strtab->original_copy + idx;
  return str;
}

const char *cstr_truncate(const char *str, size_t max_len) {
  static char buf[256];
  if (strlen(str) < max_len) {
    return str;
  }
  snprintf(buf, sizeof(buf), "%.*s...", (int)max_len, str);
  return buf;
}

int main(int argc, char *argv[]) {
  if (pd_init() == -1) {
    printf("Failed to initialize Pandora. Is the kernel extension loaded?\n");
    return 1;
  }

  if (!pd_get_kernel_base()) {
    printf("Failed to get kernel base address. Make sure SIP is disabled.\n");
    pd_deinit();
    return 1;
  }

  // make a memdiff view of the kernel mach-o header
  memdiff_view *kmhdr = memdiff_create(pd_kbase, sizeof(struct mach_header_64));

  struct mach_header_64 *header = (struct mach_header_64 *)kmhdr->original_copy;
  if (header->magic != MH_MAGIC_64) {
    printf("Kernel mach-o header has invalid magic: 0x%08x\n", header->magic);
    pd_deinit();
    return 1;
  }

  uint64_t ncmds = header->ncmds;
  uint64_t sizeofcmds = header->sizeofcmds;
  printf("kmacho->ncmds      = 0x%x\n", header->ncmds);
  printf("kmacho->sizeofcmds = 0x%x\n", header->sizeofcmds);

  if (ncmds == 0 || sizeofcmds == 0) {
    printf("Kernel mach-o header has invalid ncmds or sizeofcmds: ncmds=0x%llx "
           "sizeofcmds=0x%llx\n",
           (unsigned long long)ncmds, (unsigned long long)sizeofcmds);
    pd_deinit();
    return 1;
  }

  // make a memdiff view of the kernel mach-o load commands
  memdiff_view *kmcmd =
      memdiff_create(pd_kbase + sizeof(struct mach_header_64), sizeofcmds);

  // symtab stuff
  uint32_t nsyms = 0;
  uint64_t symoff = 0;
  uint64_t stroff = 0;
  uint64_t strsize = 0;
  bool saw_symtab = false;
  memdiff_view *kmsymn; // symbol nlist
  memdiff_view *kmsyms; // symbol strtable

  segment_map *segs = calloc(ncmds, sizeof(*segs));
  size_t seg_count = 0;
  if (!segs) {
    printf("Failed to allocate segment map\n");
    memdiff_destroy(kmhdr);
    memdiff_destroy(kmcmd);
    pd_deinit();
    return 1;
  }

  struct load_command *load_cmd = (struct load_command *)kmcmd->original_copy;
  for (uint64_t i = 0; i < ncmds; i++) {
    switch (load_cmd->cmd) {
    case LC_SEGMENT_64:
      struct segment_command_64 *seg = (struct segment_command_64 *)load_cmd;
      if (seg_count < ncmds) {
        segs[seg_count].fileoff = seg->fileoff;
        segs[seg_count].filesize = seg->filesize;
        segs[seg_count].vmaddr = seg->vmaddr;
        seg_count++;
      }
      break;
    case LC_SYMTAB:
      struct symtab_command *symtab = (struct symtab_command *)load_cmd;
      nsyms = symtab->nsyms;
      symoff = symtab->symoff;
      stroff = symtab->stroff;
      strsize = symtab->strsize;
      saw_symtab = true;
      break;
    }

    load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
  }

  if (!saw_symtab) {
    printf("Failed to find LC_SYMTAB\n");
    free(segs);
    memdiff_destroy(kmhdr);
    memdiff_destroy(kmcmd);
    pd_deinit();
    return 1;
  }

  uint64_t sym_vmaddr = fileoff_to_vmaddr(segs, seg_count, symoff);
  uint64_t str_vmaddr = fileoff_to_vmaddr(segs, seg_count, stroff);
  if (sym_vmaddr == 0 || str_vmaddr == 0) {
    printf("Failed to translate symtab file offsets: symoff=0x%llx stroff=0x%llx\n",
           (unsigned long long)symoff, (unsigned long long)stroff);
    free(segs);
    memdiff_destroy(kmhdr);
    memdiff_destroy(kmcmd);
    pd_deinit();
    return 1;
  }

  kmsymn = memdiff_create(sym_vmaddr, (size_t)nsyms * sizeof(struct nlist_64));
  kmsyms = memdiff_create(str_vmaddr, (size_t)strsize);

  if (!kmsymn || !kmsyms) {
    printf("Failed to create memdiff views for symbol table\n");
    free(segs);
    memdiff_destroy(kmhdr);
    memdiff_destroy(kmcmd);
    pd_deinit();
    return 1;
  }

  struct nlist_64 *symn = (struct nlist_64 *)kmsymn->original_copy;

  uint64_t kernproc_addr_var = 0;

  for (int i = 0; i < nsyms; i++) {
    if (strcmp(string_by_strx(symn[i].n_un.n_strx, kmsyms), "_kernproc") == 0) {
      printf("Found _kernproc at index %d: n_value=0x%llx\n", i,
             (unsigned long long)symn[i].n_value);
      kernproc_addr_var = symn[i].n_value;
    }
  }

  if (kernproc_addr_var == 0) {
    printf("Failed to find _kernproc symbol\n");
    free(segs);
    memdiff_destroy(kmhdr);
    memdiff_destroy(kmcmd);
    memdiff_destroy(kmsymn);
    memdiff_destroy(kmsyms);
    pd_deinit();
    return 1;
  }

  uint64_t kernproc_addr = pd_read64(kernproc_addr_var);
  printf("_kernproc addr: 0x%llx\n", (unsigned long long)kernproc_addr);

  memdiff_view *kernproc_view = memdiff_create(kernproc_addr, PROC_STRUCT_SIZE_DEFAULT);
  ks_proc_t kernproc_v = (ks_proc_t)kernproc_view->original_copy;

  printf("kernproc->p_pid: %d\n", kernproc_v->p_pid);
  printf("kernproc->p_comm: %s\n", kernproc_v->p_forkcopy.p_comm);

  ks_task_t kerntask_t = proc_to_task_unchecked(kernproc_v);
  hexdump(kerntask_t, sizeof(struct ks_task), default_hexdump_opts);

  memdiff_view *kernmap_view = memdiff_create((uint64_t)kerntask_t->map, sizeof(struct ks__vm_map));


  //     pid_t target_pid = getpid();
  // if (argc < 2) {
  //   printf("using self pid %d\n", target_pid);
  // } else {
  //   target_pid = (pid_t)atoi(argv[1]);
  //   printf("using pid %d from cmdline\n", target_pid);
  // }

  // uint64_t kcall_args[1] = {0};

  // uint64_t kproc = 0;
  // kcall_args[0] = (uint64_t)(int64_t)target_pid;
  // (void)pd_kcall_simple(FUNC_PROC_FIND, kcall_args, 1, &kproc);
  // if (!kproc) {
  //   printf("proc_find returned NULL for pid %d\n", target_pid);
  //   pd_deinit();
  //   return 1;
  // }

  // uint64_t proc_struct_size = pd_read64(PROC_STRUCT_SIZE_PTR);
  // if (!proc_struct_size) {
  //   proc_struct_size = PROC_STRUCT_SIZE_DEFAULT;
  // }
  // if (proc_struct_size > PROC_STRUCT_SIZE_MAX) {
  //   printf("unexpected proc struct size: 0x%llx\n",
  //          (unsigned long long)proc_struct_size);
  //   proc_struct_size = PROC_STRUCT_SIZE_DEFAULT;
  // }

  // uint8_t proc_flags = pd_read8(kproc + PROC_OFF_FLAGS);
  // uint64_t computed_ktask = (proc_flags & 2) ? (kproc + proc_struct_size) : 0;

  // uint64_t kernel_ktask = 0;
  // kcall_args[0] = kproc;
  // (void)pd_kcall_simple(FUNC_PROC_TASK, kcall_args, 1, &kernel_ktask);
  // if (kernel_ktask != computed_ktask) {
  //   printf("proc_task mismatch:\n\tcomputed: 0x%llx\n\tkernel:   0x%llx\n",
  //          (unsigned long long)computed_ktask,
  //          (unsigned long long)kernel_ktask);
  // }

  // uint64_t proc_ro_ptr = pd_read64(kproc + PROC_OFF_RO_PTR);

  // if (computed_ktask) {
  //   uint64_t task_ro_ptr = pd_read64(computed_ktask + TASK_OFF_RO_PTR);
  //   printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask @ 0x%llx:\n\tro_ptr: "
  //          "0x%llx\n",
  //          (unsigned long long)kproc, (unsigned long long)proc_ro_ptr,
  //          (unsigned long long)computed_ktask, (unsigned long long)task_ro_ptr);
  // } else {
  //   printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask: <none> (proc+0x%x "
  //          "flags=0x%02x)\n",
  //          (unsigned long long)kproc, (unsigned long long)proc_ro_ptr,
  //          (unsigned int)PROC_OFF_FLAGS, (unsigned int)proc_flags);
  // }

  // uint64_t ro_for_proc = 0;
  // kcall_args[0] = kproc;
  // (void)pd_kcall_simple(FUNC_RO_FOR_PROC, kcall_args, 1, &ro_for_proc);
  // printf("ro for proc kproc=0x%llx: 0x%llx\n", (unsigned long long)kproc,
  //        (unsigned long long)ro_for_proc);

  // bool proc_ro_valid = true;
  // bool task_ro_valid = true;

  // printf("Validating proc_ro region...\n");
  // if (!proc_ro_ptr) {
  //   printf("\tproc_ro_ptr is NULL\n");
  // } else {
  //   uint64_t proc_ro_proc = pd_read64(proc_ro_ptr);
  //   uint64_t proc_ro_task = pd_read64(proc_ro_ptr + 8);
  //   printf("\tproc_ro raw: proc=0x%llx task=0x%llx\n",
  //          (unsigned long long)proc_ro_proc, (unsigned long long)proc_ro_task);
  //   if (proc_ro_proc != kproc) {
  //     printf("\tInvalid proc ro region: proc_ro_proc != kproc\n\tInvalid proc "
  //            "ro region: 0x%llx != 0x%llx\n",
  //            (unsigned long long)proc_ro_proc, (unsigned long long)kproc);
  //     proc_ro_valid = false;
  //   }
  //   if (proc_ro_task != computed_ktask) {
  //     printf("\tInvalid proc ro region: proc_ro_task != ktask\n\tInvalid proc "
  //            "ro region: 0x%llx != 0x%llx\n",
  //            (unsigned long long)proc_ro_task,
  //            (unsigned long long)computed_ktask);
  //     proc_ro_valid = false;
  //   }
  // }

  // if (computed_ktask) {
  //   printf("Validating task_ro region...\n");
  //   uint64_t task_ro_ptr = pd_read64(computed_ktask + TASK_OFF_RO_PTR);
  //   if (task_ro_ptr) {
  //     uint64_t task_ro_proc = pd_read64(task_ro_ptr);
  //     uint64_t task_ro_task = pd_read64(task_ro_ptr + 8);
  //     printf("\ttask_ro raw: proc=0x%llx task=0x%llx\n",
  //            (unsigned long long)task_ro_proc,
  //            (unsigned long long)task_ro_task);
  //     if (task_ro_proc != kproc) {
  //       printf("\tInvalid task ro region: task_ro_proc != kproc\n\tInvalid "
  //              "task ro region: 0x%llx != 0x%llx\n",
  //              (unsigned long long)task_ro_proc, (unsigned long long)kproc);
  //       proc_ro_valid = false;
  //     }
  //     if (task_ro_task != computed_ktask) {
  //       printf("\tInvalid task ro region: task_ro_task != ktask\n\tInvalid "
  //              "task ro region: 0x%llx != 0x%llx\n",
  //              (unsigned long long)task_ro_task,
  //              (unsigned long long)computed_ktask);
  //       proc_ro_valid = false;
  //     }
  //   } else {
  //     printf("\ttask_ro_ptr is NULL\n");
  //   }
  // }

  // if (proc_ro_valid || task_ro_valid) {
  //   dump_ro_info(proc_ro_ptr); // prefer proc ro ptr for convenience
  // } else {
  //   printf("Neither proc_ro nor task_ro regions are valid!\n");
  // }

  // kcall_args[0] = kproc;
  // (void)pd_kcall_simple(FUNC_PROC_RELE, kcall_args, 1, NULL);
  // printf("released process structure\n");

  // uint64_t cproc = kproc;
  // memdiff_view *kprocview;
  // memdiff_view *kprocroview;

  // char buf[512];

  // int total_procs = 0;
  // int procs_w_hardened = 0;

  // while (cproc != 0) {
  //   memdiff_view *kprocview = memdiff_create(cproc, sizeof(struct ks_proc));
  //   if (!kprocview) {
  //     printf("failed to create memdiff view for kproc\n");
  //     total_procs++;
  //     goto end;
  //   }

  //   ks_proc_t kproc_v = (ks_proc_t)kprocview->original_copy;

  //   // make a view of the ro region
  //   kprocroview = memdiff_create((uintptr_t)kproc_v->p_proc_ro,
  //                                sizeof(struct ks_proc_ro));
  //   if (!kprocroview) {
  //     printf("failed to create memdiff view for kproc ro\n");
  //     free(kprocview);
  //     total_procs++;
  //     goto end;
  //   }

  //   ks_proc_ro_t kproc_ro_v = (ks_proc_ro_t)kprocroview->original_copy;

  //   if ((kproc_ro_v->proc_data.p_csflags & CS_RUNTIME)) {
  //     free(kprocroview);
  //     free(kprocview);
  //     cproc = (uint64_t)kproc_v->p_list.le_prev;
  //     total_procs++;
  //     continue;
  //   }

  //   memset(buf, 0, sizeof(buf));
  //   proc_ro_p_csflags_description(kproc_ro_v->proc_data.p_csflags, buf,
  //                                 sizeof(buf));

  //   printf("%s... (%i)\n\t%s\n", kproc_v->p_forkcopy.p_comm,
  //          (int)kproc_v->p_pid, buf);
  //   cproc = (uint64_t)kproc_v->p_list.le_prev;

  //   procs_w_hardened++;
  //   total_procs++;

  //   free(kprocroview);
  //   free(kprocview);
  // }

  // printf("%i/%i procs hardened", procs_w_hardened, total_procs);

end:
  pd_deinit();

  return 0;
}
