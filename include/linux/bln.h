/* include/linux/bln.h */

#ifndef _LINUX_BLN_H
#define _LINUX_BLN_H

#include <linux/types.h>


struct bln_implementation {
	void (*led_set)(int value);
};


void register_bln_implementation(struct bln_implementation *imp);
bool bln_is_ongoing(void);
int get_bln_brightness(void);
#endif
