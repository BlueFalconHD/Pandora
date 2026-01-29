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

#include "esym/b.h"
#include "esym/nop.h"

// -----------------------------------------------------------------------
// Helpers (C, not C++): patch TBNZ -> NOP and TBZ -> B
// -----------------------------------------------------------------------
static bool patch_tbnz_to_nop(csh handle, uint64_t pc) {
  printf("🔍 Patching TBNZ to NOP at 0x%llx\n", pc);
  uint32_t original = pd_read32(pc);
  cs_insn *insn = NULL;
  size_t n = cs_disasm(handle, (uint8_t *)&original, 4, pc, 1, &insn);
  if (n != 1 || insn[0].id != ARM64_INS_TBNZ) {
    printf("    ❌ Expected TBNZ at 0x%llx, found \"%s\"\n", pc,
           n ? insn[0].mnemonic : "???");
    if (insn)
      cs_free(insn, n);
    return false;
  }
  printf("    🌀 Patching TBNZ @0x%llx => NOP\n", pc);
  uint32_t nop = encode_nop();
  pd_write32(pc, nop);
  bool ok = (pd_read32(pc) == nop);
  puts(ok ? "    😎 Patch OK" : "    ❌ Verification failed");
  cs_free(insn, n);
  return ok;
}

static bool patch_tbz_to_b(csh handle, uint64_t pc) {
  printf("🔍 Patching TBZ to B at 0x%llx\n", pc);
  uint32_t original = pd_read32(pc);
  cs_insn *insn = NULL;
  size_t n = cs_disasm(handle, (uint8_t *)&original, 4, pc, 1, &insn);
  if (n != 1 || insn[0].id != ARM64_INS_TBZ) {
    printf("     ❌ Expected TBZ at 0x%llx, found \"%s\"\n", pc,
           n ? insn[0].mnemonic : "???");
    if (insn)
      cs_free(insn, n);
    return false;
  }
  const cs_arm64 *a64 = &insn[0].detail->arm64;
  if (a64->op_count < 3 || a64->operands[2].type != ARM64_OP_IMM) {
    puts("    ❌ Capstone did not supply a branch target");
    cs_free(insn, n);
    return false;
  }
  uint64_t target = (uint64_t)a64->operands[2].imm;
  if ((target & 3ULL) != 0ULL) {
    printf("    ❌ Target 0x%llx is not word-aligned\n", target);
    cs_free(insn, n);
    return false;
  }
  uint32_t branch = encode_b_to(pc, target); /* B target */
  printf("    🌀 Patching TBZ @0x%llx => B 0x%llx (0x%08x)\n", pc, target,
         branch);
  pd_write32(pc, branch);
  bool ok = (pd_read32(pc) == branch);
  puts(ok ? "    😎 Patch OK" : "    ❌ Verification failed");
  cs_free(insn, n);
  return ok;
}

// constants as provided (unslid + kslide)
#define SUB_THREAD_SET_STATE_INTERNAL kslide(0xFFFFFE00088DCD18ULL)
#define OFFSET_TBNZ_ENTITLEMENT_CHECK 0x44u /* TBNZ  W6,#9  */
#define OFFSET_TBZ_THREAD_FLAG 0x4Cu        /* TBZ   W8,#31 */

typedef struct proc_t__ *kproc_t;
typedef struct task_t__ *ktask_t;

int main(int argc, char *argv[]) {
  if (pd_init() == -1) {
    printf("Failed to initialize Pandora. Is the kernel extension loaded?\n");
    return 1;
  }

  uint64_t kernelBase = pd_get_kernel_base();

  if (kernelBase == 0) {
    printf("Failed to get kernel base address. Make sure SIP is disabled.\n");
    pd_deinit();
    return 1;
  }

  // if (getenv("PANDORA_SMOKE_KCALL")) {
  //   uint64_t ret0 = 0;
  //   kern_return_t kr = pd_kcall_simple(0, NULL, 0, &ret0);
  //   printf("kcall smoke: status=0x%x ret0=0x%llx\n", kr,
  //          (unsigned long long)ret0);
  // }

  // if (getenv("PANDORA_SMOKE_RUN_ARB")) {
  //   uint64_t ret0 = 0;
  //   kern_return_t kr =
  //       pd_run_arb_func_with_task_arg_pid(0, getpid(), &ret0);
  //   printf("run_arb smoke: status=0x%x ret0=0x%llx\n", kr,
  //          (unsigned long long)ret0);
  // }

  // printf("\nthread_set_state entitlement bypass\n"
  //        "Changes vanish on reboot. Proceed?  (y/N): ");
  // char reply = 0;
  // scanf(" %c", &reply);
  // if (reply == 'y' || reply == 'Y') {
  //   csh handle;
  //   if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle) != CS_ERR_OK) {
  //     printf("❌ Capstone init failed\n");
  //   } else {
  //     cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
  //     bool ok1 = patch_tbnz_to_nop(handle, SUB_THREAD_SET_STATE_INTERNAL +
  //                                   OFFSET_TBNZ_ENTITLEMENT_CHECK);
  //     bool ok2 = patch_tbz_to_b(handle, SUB_THREAD_SET_STATE_INTERNAL +
  //                                OFFSET_TBZ_THREAD_FLAG);
  //     cs_close(&handle);
  //     if (ok1 && ok2) {
  //       puts("🎉 All patches applied successfully!");
  //     } else {
  //       puts("💔 One or more patches failed.");
  //     }
  //   }
  // } else {
  //   puts("📭 Skipped thread_set_state entitlement bypass.");
  // }

  uint64_t ret0;

  pid_t tpid = getpid();

  if (argc > 1) {
    tpid = (pid_t)atoi(argv[1]);
    printf("using pid %d from cmdline\n", tpid);
  } else {
      printf("using self pid %d\n", tpid);
  }

#define FUNC_FIND_PROC kslide(0xFFFFFE0008DFEE7CULL)
#define FUNC_GET_ROFLAGS kslide(0xFFFFFE00088C2C10ULL)
#define FUNC_SET_ROFLAGS_HARDEN_BIT kslide(0xFFFFFE00088C41C8ULL)

#define proc_struct_size 0x7A0

  kproc_t target_kproc_obj;

  uint64_t args[1];
  args[0] = (uint64_t)(int64_t)tpid;

  pd_kcall_simple(FUNC_FIND_PROC, args, 1, (uint64_t *)&target_kproc_obj);

  printf("found kproc obj at 0x%llx\n", (unsigned long long)target_kproc_obj);

  // task for proc is (proc_ptr + proc_struct_size )
  ktask_t ttask = (ktask_t)((uint64_t)target_kproc_obj + proc_struct_size);

  printf("found task obj at 0x%llx\n", (unsigned long long)ttask);

  #define TASK_STRUCT_BSDINFO_RO_OFF 0x3E8

  // kread at task + bsdinfo_ro_off
  uint64_t bsdinfo_ro = pd_read64((uint64_t)ttask + TASK_STRUCT_BSDINFO_RO_OFF);

  printf("found bsdinfo_ro at 0x%llx\n", (unsigned long long)bsdinfo_ro);

  uint64_t ro_proc_ptr = pd_read64(bsdinfo_ro);
    printf("found ro_proc_ptr at 0x%llx\n", (unsigned long long)ro_proc_ptr);
  uint64_t ro_task_ptr = pd_read64(ro_proc_ptr + sizeof(uint64_t));
  printf("found ro_task_ptr at 0x%llx\n", (unsigned long long)ro_task_ptr);

  pd_deinit();

  printf("deiniting pandora\n");
  return 0;
}
