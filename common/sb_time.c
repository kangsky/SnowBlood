#include "sb_time.h"

bool SB_compareTimeSpec(struct timespec t1, struct timespec t2) {
	if ( t1.tv_sec > t2.tv_sec ) {
		return true;
	} else if ( t1.tv_sec == t2.tv_sec ) {
		if ( t1.tv_nsec >= t2.tv_nsec ) {
			return true;
		} else {
			return false;
		}
	}
	return false;	
}
