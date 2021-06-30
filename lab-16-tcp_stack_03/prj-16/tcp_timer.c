#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list,
// find the tcp sock which stays for at 2*MSL,
// release it
void tcp_scan_timer_list() {
    struct tcp_timer *pos_tt, *q_tt;
    list_for_each_entry_safe(pos_tt, q_tt, &timer_list, list) {
        pos_tt->timeout += TCP_TIMER_SCAN_INTERVAL;
        if (pos_tt->timeout < TCP_TIMEWAIT_TIMEOUT) continue;

        list_delete_entry(&pos_tt->list);
        struct tcp_sock *tsk = timewait_to_tcp_sock(pos_tt);
        tcp_set_state(tsk, TCP_CLOSED);

        if (tsk->parent == NULL) tcp_bind_unhash(tsk);
        tcp_unhash(tsk);
    }
}

// set the timewait timer of a tcp sock,
// by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk) {
    tsk->timewait.type = 0; // type = timewait
    tsk->timewait.timeout = 0; // prepare for inc op
    list_add_tail(&tsk->timewait.list, &timer_list);
    tsk->ref_cnt += 1;
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg) {
    init_list_head(&timer_list);
    while (1) {
        usleep(TCP_TIMER_SCAN_INTERVAL);
        tcp_scan_timer_list();
    }

    return NULL;
}
