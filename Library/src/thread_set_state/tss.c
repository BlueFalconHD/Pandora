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
#include <ptrauth.h>

#include <mach/error.h>

#include "../esym/b.h"
#include "../esym/nop.h"

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

#define SUB_THREAD_SET_STATE_INTERNAL kslide(0xFFFFFE00088DCD18ULL)
#define OFFSET_TBNZ_ENTITLEMENT_CHECK 0x44u /* TBNZ  W6,#9  */
#define OFFSET_TBZ_THREAD_FLAG 0x4Cu        /* TBZ   W8,#31 */

void patch_tss() {
    printf("\nthread_set_state entitlement bypass\n"
           "Changes vanish on reboot. Proceed?  (y/N): ");
    char reply = 0;
    scanf(" %c", &reply);
    if (reply == 'y' || reply == 'Y') {
      csh handle;
      if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle) != CS_ERR_OK) {
        printf("❌ Capstone init failed\n");
      } else {
        cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
        bool ok1 = patch_tbnz_to_nop(handle, SUB_THREAD_SET_STATE_INTERNAL +
                                      OFFSET_TBNZ_ENTITLEMENT_CHECK);
        bool ok2 = patch_tbz_to_b(handle, SUB_THREAD_SET_STATE_INTERNAL +
                                   OFFSET_TBZ_THREAD_FLAG);
        cs_close(&handle);
        if (ok1 && ok2) {
          puts("🎉 All patches applied successfully!");
        } else {
          puts("💔 One or more patches failed.");
        }
      }
    } else {
      puts("📭 Skipped thread_set_state entitlement bypass.");
    }
}
