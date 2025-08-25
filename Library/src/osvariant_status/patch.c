#include "patch.h"
#include "../pandora.h"
#include "offsets.h"

#include <capstone.h>

static inline int32_t sext19(uint32_t x) { return (int32_t)(x << 13) >> 13; }

bool patch_osvariant_status(char **errorMessage) {
  csh handle;
  if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle) != CS_ERR_OK) {
    *errorMessage = strdup("Failed to initialize Capstone disassembler");
    return false;
  }
  cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

  // disassemble instruction at off_sysctl_handler_write_branch_instruction
  uint32_t instruction;
  pd_readbuf(off_sysctl_handler_write_branch_instruction, &instruction,
             sizeof(instruction));
  printf("Instruction at %llx: 0x%08x\n",
         off_sysctl_handler_write_branch_instruction, instruction);

  cs_insn *insn;
  size_t n = cs_disasm(handle, (uint8_t *)&instruction, sizeof(instruction),
                       off_sysctl_handler_write_branch_instruction, 0, &insn);

  if (n == 0) {
    *errorMessage = strdup("Failed to disassemble instruction");
    cs_close(&handle);
    return false;
  }

  // Validate B.cond encoding: 01010100 imm19 0 cond
  if (((instruction >> 24) & 0xFFu) != 0x54u) {
    *errorMessage = strdup("Instruction is not a B.cond (top byte != 0x54)");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }
  if (((instruction >> 4) & 1u) != 0) {
    *errorMessage = strdup("Instruction encodes BC.cond (bit4=1), unsupported");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  const cs_arm64 *a64 = &insn->detail->arm64;

  if (a64->op_count < 1 || a64->operands[0].type != ARM64_OP_IMM) {
    *errorMessage =
        strdup("Branch instruction does not have an immediate operand");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  uint64_t target = (uint64_t)a64->operands[0].imm;
  if (target != off_sysctl_handler_write_branch_target_old) {
    *errorMessage = strdup("Branch target does not match expected old target");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  printf("Capstone parsed target of branch: 0x%llx\n", target);

  // patch branch target to off_sysctl_handler_write_branch_target_new

  // compute new imm19 based on difference between old and new targets (B.cond)
  int32_t old_imm19 = sext19((instruction >> 5) & 0x7FFFFu);

  printf("Self-calculated branch imm19: 0x%x\n", old_imm19);

  int64_t delta =
      (int64_t)off_sysctl_handler_write_branch_target_new - (int64_t)target;

  printf("Delta to new target: 0x%llx\n", delta);

  // ensure 4-byte alignment
  if ((delta & 0x3) != 0) {
    *errorMessage = strdup("New branch target is not 4-byte aligned");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  int32_t new_imm19 = old_imm19 + (int32_t)(delta >> 2);

  printf("New branch imm19: 0x%x\n", new_imm19);

  // range check for signed 19-bit immediate
  if (new_imm19 < -(1 << 18) || new_imm19 > ((1 << 18) - 1)) {
    *errorMessage =
        strdup("New branch target is out of range for B.cond instruction");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  uint32_t patched =
      (instruction & 0xFF00001Fu) | (((uint32_t)new_imm19 & 0x7FFFFu) << 5);

  printf("New instruction to write: 0x%08x\n", patched);

  pd_writebuf(off_sysctl_handler_write_branch_instruction, &patched,
              sizeof(patched));

  // verify write
  uint32_t verify = 0;
  pd_readbuf(off_sysctl_handler_write_branch_instruction, &verify,
             sizeof(verify));
  if (verify != patched) {
    *errorMessage = strdup("Failed to patch branch instruction");
    cs_free(insn, n);
    cs_close(&handle);
    return false;
  }

  cs_free(insn, n);

  // disassemble and patch mov instruction that sets return status
  uint32_t mov_instruction = 0;
  pd_readbuf(off_sysctl_handler_return_status_instruction, &mov_instruction,
             sizeof(mov_instruction));
  cs_insn *mov_insn;
  size_t n2 =
      cs_disasm(handle, (uint8_t *)&mov_instruction, sizeof(mov_instruction),
                off_sysctl_handler_return_status_instruction, 0, &mov_insn);
  if (n2 == 0) {
    *errorMessage = strdup("Failed to disassemble return status instruction");
    cs_close(&handle);
    return false;
  }

  const cs_arm64 *mov_a64 = &mov_insn->detail->arm64;
  bool shape_ok = false;
  if (mov_insn->id == ARM64_INS_MOV || mov_insn->id == ARM64_INS_MOVZ) {
    if (mov_a64->op_count >= 2 && mov_a64->operands[0].type == ARM64_OP_REG &&
        mov_a64->operands[1].type == ARM64_OP_IMM &&
        mov_a64->operands[0].reg == ARM64_REG_W0 &&
        mov_a64->operands[1].imm == 1) {
      shape_ok = true;
    }
  } else if (mov_insn->id == ARM64_INS_ORR) {
    if (mov_a64->op_count >= 3 && mov_a64->operands[0].type == ARM64_OP_REG &&
        mov_a64->operands[1].type == ARM64_OP_REG &&
        mov_a64->operands[2].type == ARM64_OP_IMM &&
        mov_a64->operands[0].reg == ARM64_REG_W0 &&
        mov_a64->operands[1].reg == ARM64_REG_WZR &&
        mov_a64->operands[2].imm == 1) {
      shape_ok = true;
    }
  }
  if (!shape_ok) {
    *errorMessage = strdup("Return status instruction is not 'mov w0, #1'");
    cs_free(mov_insn, n2);
    cs_close(&handle);
    return false;
  }

  // patch to 'mov w0, #0' (MOVZ W0, #0)
  uint32_t mov_patched = 0x52800000u;
  pd_writebuf(off_sysctl_handler_return_status_instruction, &mov_patched,
              sizeof(mov_patched));

  // verify write
  uint32_t mov_verify = 0;
  pd_readbuf(off_sysctl_handler_return_status_instruction, &mov_verify,
             sizeof(mov_verify));
  if (mov_verify != mov_patched) {
    *errorMessage = strdup("Failed to patch return status MOV instruction");
    cs_free(mov_insn, n2);
    cs_close(&handle);
    return false;
  }

  cs_free(mov_insn, n2);
  cs_close(&handle);

  return true;
};
