#ifndef PROC_RO_H
#define PROC_RO_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

struct proc;
struct task;
struct ucred;

typedef struct {
    uint32_t val[2];
} my_security_token_t;

typedef struct {
    uint32_t val[8];
} my_audit_token_t;

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
    my_security_token_t sec_token; // 0x00 (8)
    my_audit_token_t    audit_token; // 0x08 (32)
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

    #define CS_VALID                    0x00000001  /* dynamically valid */
    #define CS_ADHOC                    0x00000002  /* ad hoc signed */
    #define CS_GET_TASK_ALLOW           0x00000004  /* has get-task-allow entitlement */
    #define CS_INSTALLER                0x00000008  /* has installer entitlement */

    #define CS_FORCED_LV                0x00000010  /* Library Validation required by Hardened System Policy */
    #define CS_INVALID_ALLOWED          0x00000020  /* (macOS Only) Page invalidation allowed by task port policy */

    #define CS_HARD                     0x00000100  /* don't load invalid pages */
    #define CS_KILL                     0x00000200  /* kill process if it becomes invalid */
    #define CS_CHECK_EXPIRATION         0x00000400  /* force expiration checking */
    #define CS_RESTRICT                 0x00000800  /* tell dyld to treat restricted */

    #define CS_ENFORCEMENT              0x00001000  /* require enforcement */
    #define CS_REQUIRE_LV               0x00002000  /* require library validation */
    #define CS_ENTITLEMENTS_VALIDATED   0x00004000  /* code signature permits restricted entitlements */
    #define CS_NVRAM_UNRESTRICTED       0x00008000  /* has com.apple.rootless.restricted-nvram-variables.heritable entitlement */

    #define CS_RUNTIME                  0x00010000  /* Apply hardened runtime policies */
    #define CS_LINKER_SIGNED            0x00020000  /* Automatically signed by the linker */

    #define CS_ALLOWED_MACHO            (CS_ADHOC | CS_HARD | CS_KILL | CS_CHECK_EXPIRATION | \
	                             CS_RESTRICT | CS_ENFORCEMENT | CS_REQUIRE_LV | CS_RUNTIME | CS_LINKER_SIGNED)

    #define CS_EXEC_SET_HARD            0x00100000  /* set CS_HARD on any exec'ed process */
    #define CS_EXEC_SET_KILL            0x00200000  /* set CS_KILL on any exec'ed process */
    #define CS_EXEC_SET_ENFORCEMENT     0x00400000  /* set CS_ENFORCEMENT on any exec'ed process */
    #define CS_EXEC_INHERIT_SIP         0x00800000  /* set CS_INSTALLER on any exec'ed process */

    #define CS_KILLED                   0x01000000  /* was killed by kernel for invalidity */
    #define CS_NO_UNTRUSTED_HELPERS     0x02000000  /* kernel did not load a non-platform-binary dyld or Rosetta runtime */
    #define CS_DYLD_PLATFORM            CS_NO_UNTRUSTED_HELPERS /* old name */
    #define CS_PLATFORM_BINARY          0x04000000  /* this is a platform binary */
    #define CS_PLATFORM_PATH            0x08000000  /* platform binary by the fact of path (osx only) */

    #define CS_DEBUGGED                 0x10000000  /* process is currently or has previously been debugged and allowed to run with invalid pages */
    #define CS_SIGNED                   0x20000000  /* process has a signature (may have gone invalid) */
    #define CS_DEV_CODE                 0x40000000  /* code is dev signed, cannot be loaded into prod signed code (will go away with rdar://problem/28322552) */
    #define CS_DATAVAULT_CONTROLLER     0x80000000  /* has Data Vault controller entitlement */

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

    #define TFRO_CORPSE                     0x00000020                      /* task is a corpse */
    #define TFRO_MACH_HARDENING_OPT_OUT     0x00000040                      /* task might load third party plugins on macOS and should be opted out of mach hardening */
    #define TFRO_PLATFORM                   0x00000080                      /* task is a platform binary */
    #define TFRO_FILTER_MSG                 0x00004000                      /* task calls into message filter callback before sending a message */
    #define TFRO_PAC_EXC_FATAL              0x00010000                      /* task is marked a corpse if a PAC exception occurs */
    #define TFRO_JIT_EXC_FATAL              0x00020000                      /* kill the task on access violations from privileged JIT code */
    #define TFRO_PAC_ENFORCE_USER_STATE     0x01000000                      /* Enforce user and kernel signed thread state */
    #define TFRO_HAS_KD_ACCESS              0x02000000                      /* Access to the kernel exclave resource domain  */
    #define TFRO_FREEZE_EXCEPTION_PORTS     0x04000000                      /* Setting new exception ports on the task/thread is disallowed */
    #define TFRO_HAS_SENSOR_MIN_ON_TIME_ACCESS     0x08000000               /* Access to sensor minimum on time call  */

    task_control_port_options_t task_control_port_options; // 0x3C
    uint8_t                     _pad_3D[3];                // 0x3D..0x3F
} task_ro_data_t;

static inline void proc_ro__append_str(char *out, size_t out_len, size_t *pos, const char *s)
{
    if (out == NULL || out_len == 0 || pos == NULL || s == NULL) {
        return;
    }
    if (*pos >= out_len) {
        return;
    }

    int wrote = snprintf(out + *pos, out_len - *pos, "%s", s);
    if (wrote <= 0) {
        return;
    }

    size_t next = *pos + (size_t)wrote;
    *pos = (next < out_len) ? next : (out_len - 1);
}

static inline void proc_ro__append_hex32(char *out, size_t out_len, size_t *pos, uint32_t v)
{
    if (out == NULL || out_len == 0 || pos == NULL) {
        return;
    }
    if (*pos >= out_len) {
        return;
    }

    int wrote = snprintf(out + *pos, out_len - *pos, "0x%08x", v);
    if (wrote <= 0) {
        return;
    }

    size_t next = *pos + (size_t)wrote;
    *pos = (next < out_len) ? next : (out_len - 1);
}

static inline const char *proc_ro_p_csflags_description(uint32_t p_csflags, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return "";
    }

    out[0] = '\0';
    size_t pos = 0;
    uint32_t remaining = p_csflags;

#define PROC_RO__ADD_FLAG(mask, name)                                                     \
    do {                                                                                  \
        if ((p_csflags & (mask)) != 0) {                                                   \
            if (pos != 0) {                                                               \
                proc_ro__append_str(out, out_len, &pos, " | ");                           \
            }                                                                             \
            proc_ro__append_str(out, out_len, &pos, (name));                              \
            remaining &= ~(uint32_t)(mask);                                               \
        }                                                                                 \
    } while (0)

    PROC_RO__ADD_FLAG(CS_VALID, "CS_VALID");
    PROC_RO__ADD_FLAG(CS_ADHOC, "CS_ADHOC");
    PROC_RO__ADD_FLAG(CS_GET_TASK_ALLOW, "CS_GET_TASK_ALLOW");
    PROC_RO__ADD_FLAG(CS_INSTALLER, "CS_INSTALLER");
    PROC_RO__ADD_FLAG(CS_FORCED_LV, "CS_FORCED_LV");
    PROC_RO__ADD_FLAG(CS_INVALID_ALLOWED, "CS_INVALID_ALLOWED");
    PROC_RO__ADD_FLAG(CS_HARD, "CS_HARD");
    PROC_RO__ADD_FLAG(CS_KILL, "CS_KILL");
    PROC_RO__ADD_FLAG(CS_CHECK_EXPIRATION, "CS_CHECK_EXPIRATION");
    PROC_RO__ADD_FLAG(CS_RESTRICT, "CS_RESTRICT");
    PROC_RO__ADD_FLAG(CS_ENFORCEMENT, "CS_ENFORCEMENT");
    PROC_RO__ADD_FLAG(CS_REQUIRE_LV, "CS_REQUIRE_LV");
    PROC_RO__ADD_FLAG(CS_ENTITLEMENTS_VALIDATED, "CS_ENTITLEMENTS_VALIDATED");
    PROC_RO__ADD_FLAG(CS_NVRAM_UNRESTRICTED, "CS_NVRAM_UNRESTRICTED");
    PROC_RO__ADD_FLAG(CS_RUNTIME, "CS_RUNTIME");
    PROC_RO__ADD_FLAG(CS_LINKER_SIGNED, "CS_LINKER_SIGNED");
    PROC_RO__ADD_FLAG(CS_EXEC_SET_HARD, "CS_EXEC_SET_HARD");
    PROC_RO__ADD_FLAG(CS_EXEC_SET_KILL, "CS_EXEC_SET_KILL");
    PROC_RO__ADD_FLAG(CS_EXEC_SET_ENFORCEMENT, "CS_EXEC_SET_ENFORCEMENT");
    PROC_RO__ADD_FLAG(CS_EXEC_INHERIT_SIP, "CS_EXEC_INHERIT_SIP");
    PROC_RO__ADD_FLAG(CS_KILLED, "CS_KILLED");
    PROC_RO__ADD_FLAG(CS_NO_UNTRUSTED_HELPERS, "CS_NO_UNTRUSTED_HELPERS");
    PROC_RO__ADD_FLAG(CS_PLATFORM_BINARY, "CS_PLATFORM_BINARY");
    PROC_RO__ADD_FLAG(CS_PLATFORM_PATH, "CS_PLATFORM_PATH");
    PROC_RO__ADD_FLAG(CS_DEBUGGED, "CS_DEBUGGED");
    PROC_RO__ADD_FLAG(CS_SIGNED, "CS_SIGNED");
    PROC_RO__ADD_FLAG(CS_DEV_CODE, "CS_DEV_CODE");
    PROC_RO__ADD_FLAG(CS_DATAVAULT_CONTROLLER, "CS_DATAVAULT_CONTROLLER");

#undef PROC_RO__ADD_FLAG

    if (pos == 0) {
        if (p_csflags == 0) {
            proc_ro__append_str(out, out_len, &pos, "0");
        } else {
            proc_ro__append_hex32(out, out_len, &pos, p_csflags);
        }
        return out;
    }

    if (remaining != 0) {
        proc_ro__append_str(out, out_len, &pos, " | ");
        proc_ro__append_hex32(out, out_len, &pos, remaining);
    }

    return out;
}

static inline const char *proc_ro_t_flags_ro_description(uint32_t t_flags_ro, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return "";
    }

    out[0] = '\0';
    size_t pos = 0;
    uint32_t remaining = t_flags_ro;

#define PROC_RO__ADD_FLAG(mask, name)                                                     \
    do {                                                                                  \
        if ((t_flags_ro & (mask)) != 0) {                                                  \
            if (pos != 0) {                                                               \
                proc_ro__append_str(out, out_len, &pos, " | ");                           \
            }                                                                             \
            proc_ro__append_str(out, out_len, &pos, (name));                              \
            remaining &= ~(uint32_t)(mask);                                               \
        }                                                                                 \
    } while (0)

    PROC_RO__ADD_FLAG(TFRO_CORPSE, "TFRO_CORPSE");
    PROC_RO__ADD_FLAG(TFRO_MACH_HARDENING_OPT_OUT, "TFRO_MACH_HARDENING_OPT_OUT");
    PROC_RO__ADD_FLAG(TFRO_PLATFORM, "TFRO_PLATFORM");
    PROC_RO__ADD_FLAG(TFRO_FILTER_MSG, "TFRO_FILTER_MSG");
    PROC_RO__ADD_FLAG(TFRO_PAC_EXC_FATAL, "TFRO_PAC_EXC_FATAL");
    PROC_RO__ADD_FLAG(TFRO_JIT_EXC_FATAL, "TFRO_JIT_EXC_FATAL");
    PROC_RO__ADD_FLAG(TFRO_PAC_ENFORCE_USER_STATE, "TFRO_PAC_ENFORCE_USER_STATE");
    PROC_RO__ADD_FLAG(TFRO_HAS_KD_ACCESS, "TFRO_HAS_KD_ACCESS");
    PROC_RO__ADD_FLAG(TFRO_FREEZE_EXCEPTION_PORTS, "TFRO_FREEZE_EXCEPTION_PORTS");
    PROC_RO__ADD_FLAG(TFRO_HAS_SENSOR_MIN_ON_TIME_ACCESS, "TFRO_HAS_SENSOR_MIN_ON_TIME_ACCESS");

#undef PROC_RO__ADD_FLAG

    if (pos == 0) {
        if (t_flags_ro == 0) {
            proc_ro__append_str(out, out_len, &pos, "0");
        } else {
            proc_ro__append_hex32(out, out_len, &pos, t_flags_ro);
        }
        return out;
    }

    if (remaining != 0) {
        proc_ro__append_str(out, out_len, &pos, " | ");
        proc_ro__append_hex32(out, out_len, &pos, remaining);
    }

    return out;
}

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
