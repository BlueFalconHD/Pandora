#pragma once

#include "../pandora.h"

// offset of backing store for sysctl value
#define off_sysctl_value_unslid 0xfffffe000c527cf0
#define off_sysctl_value kslide(off_sysctl_value_unslid)

// base address of sysctl handler function (unslid)
#define off_sysctl_handler_base_unslid 0xfffffe0008db786c

// offset of the branch instruction to eventually run _sysctl_handle_quad when
// write checks pass
#define off_sysctl_handler_write_branch_instruction_unslid                     \
  (off_sysctl_handler_base_unslid + 0x34)
#define off_sysctl_handler_write_branch_instruction                            \
  kslide(off_sysctl_handler_write_branch_instruction_unslid)

// target of the branch to eventually run _sysctl_handle_quad when write checks
// pass
#define off_sysctl_handler_write_branch_target_old_unslid                      \
  (off_sysctl_handler_base_unslid + 0x40)
#define off_sysctl_handler_write_branch_target_old                             \
  kslide(off_sysctl_handler_write_branch_target_old_unslid)

// new target of the branch to eventually run _sysctl_handle_quad when write
// checks are bypassed
#define off_sysctl_handler_write_branch_target_new_unslid                      \
  (off_sysctl_handler_base_unslid + 0x38)
#define off_sysctl_handler_write_branch_target_new                             \
  kslide(off_sysctl_handler_write_branch_target_new_unslid)

// address of mov instruction which sets the return status, we patch from `mov
// w0 #1` to `mov w0 #0` to fake success
#define off_sysctl_handler_return_status_instruction_unslid                    \
  (off_sysctl_handler_base_unslid + 0x38)
#define off_sysctl_handler_return_status_instruction                           \
  kslide(off_sysctl_handler_return_status_instruction_unslid)
