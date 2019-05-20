#ifndef SB_TIME_H
#define SB_TIME_H

#include "time.h"
#include "stdbool.h"

// if t1 is larger than or equal to t2, return TRUE; otherwise, return FALSE
bool SB_compareTimeSpec(struct timespec t1, struct timespec t2);

#endif
