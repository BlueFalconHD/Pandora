#ifndef CALYPSO_PAGE_SIZE_H
#define CALPYSO_PAGE_SIZE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/sysctl.h>

extern int page_size_once;
extern uint64_t page_size;

int get_page_size();

#endif
