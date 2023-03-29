#include <stdio.h>
#include "timer.h"

void add_timer(struct list_head *head, struct timer_entry *timer)
{
	struct list_head *pos;

	timer->time = timer->period;
	list_for_each(pos, head) {			// 遍历
		struct timer_entry *t = list_entry(pos, struct timer_entry, list);	//
		if (timer->time < t->time) {
			list_add_tail(&timer->list, pos);		// 插入链表前面
			t->time -= timer->time;
			INIT_LIST_HEAD(&timer->callback.list);
			return;
		} else if (timer->time == t->time) {
			list_add_tail(&timer->callback.list, &t->callback.list);
			return;
		}
		timer->time -= t->time;
	}
	INIT_LIST_HEAD(&timer->callback.list);
	list_add_tail(&timer->list, head);
}

void del_timer(struct timer_entry *timer)
{
	list_del(&timer->list);
}

void del_callback(struct timer_callback *callback)
{
	list_del(&callback->list);
}
