# pandora 'library' and tests

this will soon be moved elsewhere, but for right now the pandora library and test code which is able to actually do things with kernel memory exist in the same place.

quick note, if the test program fails to run, errors, or panics your machine, the
structure definitions for your kernel version are likely different from mine.

to fix this, you will need to regenerate the structure definitions for your kernel using [kstructs](https://github.com/BlueFalconHD/kstructs)

The KDK dSYM file used for kstructs.h currently is: `/Library/Developer/KDKs/KDK_26.2_25C5048a.kdk/System/Library/Kernels/kernel.release.t8112.dSYM/Contents/Resources/DWARF/kernel.release.t8112`

The kstructs command used to generate the file in this repo is:

```bash
build/kstructs --type proc_t,task_t,vm_map_t --max-depth 10 --type-prefix ks_ $KDK_DSYM_PATH > /tmp/t8112.h
```

## capabilities

I have written code on top of pandora's core functionality capable of doing the following things:
- scanning for xrefs (adrp+add) to a given memory address within the kernel
- finding any symbol the kernel actually exports
- parsing the kernel mach-o and finding segments + sections within

In the future, I am to have pandora (or whatever the thing built on top will be called) to be capable of:
- easily patching kernel functions through trampolines or other methods
- unrestrict processes and allow them to do whatever they want
  - thread_create_running patch??
