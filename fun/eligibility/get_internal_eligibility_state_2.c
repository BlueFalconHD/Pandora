// Filename: eligibility_state_dump.c

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <xpc/xpc.h>

typedef int (*os_eligibility_get_internal_state_t)(xpc_object_t *out_state);
typedef int (*os_eligibility_get_state_dump_t)(xpc_object_t *out_dump);

void print_xpc_object(xpc_object_t obj);

int main() {
  void *handle = NULL;
  os_eligibility_get_internal_state_t os_eligibility_get_internal_state = NULL;
  os_eligibility_get_state_dump_t os_eligibility_get_state_dump = NULL;
  xpc_object_t internal_state = NULL;
  xpc_object_t state_dump = NULL;
  int result = 0;

  // Load the libsystem_eligibility.dylib from the dyld shared cache
  handle = dlopen("/usr/lib/system/libsystem_eligibility.dylib", RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "Failed to open libsystem_eligibility.dylib: %s\n",
            dlerror());
    return 1;
  }

  // Clear any existing errors
  dlerror();

  // Get the symbol for os_eligibility_get_internal_state
  *(void **)(&os_eligibility_get_internal_state) =
      dlsym(handle, "os_eligibility_get_internal_state");
  char *error = dlerror();
  if (error != NULL) {
    fprintf(stderr, "Failed to find os_eligibility_get_internal_state: %s\n",
            error);
    dlclose(handle);
    return 1;
  }

  // Get the symbol for os_eligibility_get_state_dump
  *(void **)(&os_eligibility_get_state_dump) =
      dlsym(handle, "os_eligibility_get_state_dump");
  error = dlerror();
  if (error != NULL) {
    fprintf(stderr, "Failed to find os_eligibility_get_state_dump: %s\n",
            error);
    dlclose(handle);
    return 1;
  }

  // Call os_eligibility_get_internal_state
  result = os_eligibility_get_internal_state(&internal_state);
  if (result != 0) {
    fprintf(stderr, "os_eligibility_get_internal_state failed with error: %d\n",
            result);
    // Proceeding to try os_eligibility_get_state_dump
  } else {
    // Print the internal state
    if (internal_state) {
      printf("\n=== Internal State ===\n");
      print_xpc_object(internal_state);
      xpc_release(internal_state);
    } else {
      printf("No internal state returned.\n");
    }
  }

  // Call os_eligibility_get_state_dump
  result = os_eligibility_get_state_dump(&state_dump);
  if (result != 0) {
    fprintf(stderr, "os_eligibility_get_state_dump failed with error: %d\n",
            result);
    dlclose(handle);
    return 1;
  }

  // Print the state dump
  if (state_dump) {
    printf("\n=== State Dump ===\n");
    print_xpc_object(state_dump);
    xpc_release(state_dump);
  } else {
    printf("No state dump returned.\n");
  }

  // Clean up
  dlclose(handle);
  return 0;
}

void print_xpc_object(xpc_object_t obj) {
  if (!obj) {
    printf("NULL XPC object.\n");
    return;
  }

  // Use the built-in XPC description function for deep, pretty printing
  char *description = xpc_copy_description(obj);
  if (description) {
    printf("%s\n", description);
    free(description);
  } else {
    printf("Failed to get XPC object description.\n");
  }
}
