#ifndef PROC_RO_H
#define PROC_RO_H

#include <stddef.h>
#include <stdint.h>

struct proc;
struct task;
struct ucred;

typedef struct {
    uint32_t val[2];
} security_token_t;

typedef struct {
    uint32_t val[8];
} audit_token_t;

typedef int32_t  pid_t;


typedef uint8_t task_control_port_options_t;

typedef struct {
    struct ucred * volatile __smr_ptr;
} ucred_smr_ptr_t;

// 00000000 struct task_filter_ro_data // sizeof=0x10
typedef struct {
    uint8_t *mach_trap_filter_mask; // 0x00
    uint8_t *mach_kobj_filter_mask; // 0x08
} task_filter_ro_data_t;

// 00000000 struct task_token_ro_data // sizeof=0x28
typedef struct {
    security_token_t sec_token; // 0x00 (8)
    audit_token_t    audit_token; // 0x08 (32)
} task_token_ro_data_t;

// 00000000 struct proc_platform_ro_data // sizeof=0xC
typedef struct {
    uint32_t p_platform; // 0x00
    uint32_t p_min_sdk;  // 0x04
    uint32_t p_sdk;      // 0x08
} proc_platform_ro_data_t;

// 00000000 struct proc_ro_data // sizeof=0x38 (aligned(8))
typedef struct __attribute__((aligned(8))) {
    uint64_t            p_uniqueid;         // 0x00
    int32_t             p_idversion;        // 0x08
    pid_t               p_orig_ppid;        // 0x0C
    int32_t             p_orig_ppidversion; // 0x10
    uint32_t            p_csflags;          // 0x14
    ucred_smr_ptr_t     p_ucred;            // 0x18 (8 bytes)
    uint8_t            *syscall_filter_mask;// 0x20
    proc_platform_ro_data_t p_platform_data;// 0x28 (0x0C)
    uint8_t             _pad_34[4];         // 0x34..0x37 to reach 0x38
} proc_ro_data_t;

// 00000000 struct task_ro_data // sizeof=0x40
typedef struct __attribute__((aligned(4))) {
    task_token_ro_data_t        task_tokens;               // 0x00 (0x28)
    task_filter_ro_data_t       task_filters;              // 0x28 (0x10)
    uint32_t                    t_flags_ro;                // 0x38
    task_control_port_options_t task_control_port_options; // 0x3C
    uint8_t                     _pad_3D[3];                // 0x3D..0x3F
} task_ro_data_t;

// 00000000 struct proc_ro_0 // sizeof=0x88
typedef struct {
    struct proc *pr_proc;   // 0x00
    struct task *pr_task;   // 0x08
    proc_ro_data_t proc_data; // 0x10 (0x38)
    task_ro_data_t task_data; // 0x48 (0x40)
} proc_ro_0_t;

_Static_assert(sizeof(task_filter_ro_data_t) == 0x10, "task_filter_ro_data_t size");
_Static_assert(offsetof(task_filter_ro_data_t, mach_trap_filter_mask) == 0x00, "task_filter trap off");
_Static_assert(offsetof(task_filter_ro_data_t, mach_kobj_filter_mask) == 0x08, "task_filter kobj off");

_Static_assert(sizeof(task_token_ro_data_t) == 0x28, "task_token_ro_data_t size");
_Static_assert(offsetof(task_token_ro_data_t, sec_token) == 0x00, "task_token sec off");
_Static_assert(offsetof(task_token_ro_data_t, audit_token) == 0x08, "task_token audit off");

_Static_assert(sizeof(proc_platform_ro_data_t) == 0x0C, "proc_platform_ro_data_t size");

_Static_assert(sizeof(proc_ro_data_t) == 0x38, "proc_ro_data_t size");
_Static_assert(offsetof(proc_ro_data_t, p_uniqueid) == 0x00, "proc_ro uniqueid off");
_Static_assert(offsetof(proc_ro_data_t, p_idversion) == 0x08, "proc_ro idversion off");
_Static_assert(offsetof(proc_ro_data_t, p_orig_ppid) == 0x0C, "proc_ro orig_ppid off");
_Static_assert(offsetof(proc_ro_data_t, p_orig_ppidversion) == 0x10, "proc_ro orig_ppidversion off");
_Static_assert(offsetof(proc_ro_data_t, p_csflags) == 0x14, "proc_ro csflags off");
_Static_assert(offsetof(proc_ro_data_t, p_ucred) == 0x18, "proc_ro ucred off");
_Static_assert(offsetof(proc_ro_data_t, syscall_filter_mask) == 0x20, "proc_ro syscall_filter_mask off");
_Static_assert(offsetof(proc_ro_data_t, p_platform_data) == 0x28, "proc_ro platform_data off");

_Static_assert(sizeof(task_ro_data_t) == 0x40, "task_ro_data_t size");
_Static_assert(offsetof(task_ro_data_t, task_tokens) == 0x00, "task_ro tokens off");
_Static_assert(offsetof(task_ro_data_t, task_filters) == 0x28, "task_ro filters off");
_Static_assert(offsetof(task_ro_data_t, t_flags_ro) == 0x38, "task_ro flags off");
_Static_assert(offsetof(task_ro_data_t, task_control_port_options) == 0x3C, "task_ro portopts off");

_Static_assert(sizeof(proc_ro_0_t) == 0x88, "proc_ro_0_t size");
_Static_assert(offsetof(proc_ro_0_t, pr_proc) == 0x00, "proc_ro_0 pr_proc off");
_Static_assert(offsetof(proc_ro_0_t, pr_task) == 0x08, "proc_ro_0 pr_task off");
_Static_assert(offsetof(proc_ro_0_t, proc_data) == 0x10, "proc_ro_0 proc_data off");
_Static_assert(offsetof(proc_ro_0_t, task_data) == 0x48, "proc_ro_0 task_data off");

#endif // PROC_RO_H
