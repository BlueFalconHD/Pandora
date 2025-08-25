#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// takes in a pointer to a place to store an error message, returns true if the
// validation passed, false otherwise
bool patch_osvariant_status(char **errorMessage);
