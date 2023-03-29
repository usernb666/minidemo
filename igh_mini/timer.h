#ifndef __TIMER_H__
#define __TIMER_H__

#include "list.h"

struct timer_callback {
	struct list_head list;
	void (*callback)(void *data);
	void *data;
	struct timer_entry *timer;
};

struct timer_entry {
	char name[64];
	struct list_head list;
	unsigned int period; /* us */
	unsigned int time;   /* time in list (us) */
	unsigned char oneshot; /* oneshot timer ? */
	struct timer_callback callback;
};

void add_timer(struct list_head *head, struct timer_entry *timer);
void del_timer(struct timer_entry *timer);
void del_callback(struct timer_callback *callback);

#endif

