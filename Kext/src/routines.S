.align 4
.global _arm_kvtophys, _arbitrary_call

_arm_kvtophys:
    bti c
    mrs x2, DAIF
    msr DAIFSet, #0xF

    at s1e1r, x0
    isb sy
    mrs x1, PAR_EL1
    msr DAIF, x2

    tbnz x1, #0, L_kvtop_invalid
    bfm x1, x0, #0, #11
    and x0, x1, #0x0000ffffffffffff
    ret

L_kvtop_invalid:
    mov x0, #0
    ret

	/* 10 64-bit argument kernel call primitive
	extern "C" uint64_t arbitrary_call
	(
	    uint64_t func,
	    uint64_t arg0,
	    uint64_t arg1,
	    uint64_t arg2,
	    uint64_t arg3,
	    uint64_t arg4,
	    uint64_t arg5,
	    uint64_t arg6,
	    uint64_t arg7,
	    uint64_t arg8,
	    uint64_t arg9
	);
	*/
	_arbitrary_call:
	    bti c

	    // Load the function pointer into X16
	    mov x16, x0
	    xpaci x16

	    // Re-map wrapper args (func, arg0..arg9) to a normal kernel call ABI:
	    //   x0..x7 = arg0..arg7
	    //   [sp]   = arg8, [sp+8] = arg9
	    //
	    // On arm64, our wrapper receives:
	    //   x0=func, x1..x7=arg0..arg6, [sp]=arg7, [sp+8]=arg8, [sp+16]=arg9

	    ldr x17, [sp]         // arg7
	    ldr x15, [sp, #0x8]   // arg8
	    ldr x14, [sp, #0x10]  // arg9

	    str x15, [sp]
	    str x14, [sp, #0x8]

	    mov x0, x1
	    mov x1, x2
	    mov x2, x3
	    mov x3, x4
	    mov x4, x5
	    mov x5, x6
	    mov x6, x7
	    mov x7, x17

	    br x16
