#ifndef PROC_UTILS_H
#define PROC_UTILS_H

#include <sys/proc.h>

class ProcUtils {
public:
  /*
   * Check whether a PID exists in the system.
   */
  static bool pidExists(pid_t pid) {
    struct proc *p = proc_find(pid);
    if (p) {
      proc_rele(p);
      return true;
    }
    return false;
  }
};

#endif // PROC_UTILS_H
