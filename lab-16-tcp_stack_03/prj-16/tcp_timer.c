#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

pthread_mutex_t timer_lock;

// scan the timer_list,
// find the tcp sock which stays for at 2*MSL,
// release it
void tcp_scan_timer_list() {
    struct tcp_timer *pos_tt, *q_tt;
    pthread_mutex_lock(&timer_lock);
    list_for_each_entry_safe(pos_tt, q_tt, &timer_list, list) {
        struct tcp_sock *tsk;
        switch (pos_tt->type) {
            case 0: // time-wait
                pos_tt->timeout += TCP_TIMER_SCAN_INTERVAL;
                if (pos_tt->timeout < TCP_TIMEWAIT_TIMEOUT) continue;

                list_delete_entry(&pos_tt->list);
                tsk = timewait_to_tcp_sock(pos_tt);
                tcp_set_state(tsk, TCP_CLOSED);

                if (tsk->parent == NULL) tcp_bind_unhash(tsk);
                tcp_unhash(tsk);
                break;
            case 1: // retrans
                tsk = retranstimer_to_tcp_sock(pos_tt);
                pthread_mutex_lock(&tsk->send_lock);
                struct send_buffer *pos_buf;
                list_for_each_entry(
                        pos_buf, &tsk->send_buf, list) {
                    pos_buf->timeout -= TCP_TIMER_SCAN_INTERVAL;
                    if (!pos_buf->timeout) {
                        if (pos_buf->times++ == 3) {
                            fprintf(stdout, "[Hint] Packet Loss.\n");
                            // XXX:
                            // here you should use close, but if you do
                            // as told, the experiment would get hard

                            // TODO: might have other things to be clear
                            // tcp_sock_close(tsk);
                            // ...


                            /* infinite retrans version */
                            pos_buf->times = 1;
                            pos_buf->timeout = TCP_RETRANS_INT;

                        } else {
                            char *tmp = (char *) malloc(
                                    pos_buf->len * sizeof(char));
                            memcpy(tmp, pos_buf->packet, pos_buf->len);
                            ip_send_packet(tmp, pos_buf->len);
                            if (pos_buf->times == 2)
                                pos_buf->timeout = 2 * TCP_RETRANS_INT;
                            else
                                pos_buf->timeout = 4 * TCP_RETRANS_INT;
                        }
                    }
                }
                pthread_mutex_unlock(&tsk->send_lock);
                break;
            default:
                break;
        }
    }
    pthread_mutex_unlock(&timer_lock);
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
    // mutex for timer_list
    pthread_mutex_init(&timer_lock, NULL);
    timer_thread_init = 1;
    while (1) {
        usleep(TCP_TIMER_SCAN_INTERVAL);
        tcp_scan_timer_list();
    }

    return NULL;
}

// set retransmission timer for sock
void tcp_set_retrans_timer(struct tcp_sock *tsk) {
    tsk->retrans_timer.type = 1; // type = retrans
    tsk->retrans_timer.timeout = 0; // prepare for inc op
    pthread_mutex_lock(&timer_lock);
    struct tcp_timer *pos_tmr;
    list_for_each_entry(pos_tmr, &timer_list, list) {
        if (retranstimer_to_tcp_sock(pos_tmr) == tsk) {
            pthread_mutex_unlock(&timer_lock);
            return;
        }
    }

    list_add_tail(&tsk->retrans_timer.list, &timer_list);
    tsk->ref_cnt += 1;
    pthread_mutex_unlock(&timer_lock);
}

// unset retransmission timer for sock
void tcp_unset_retrans_timer(struct tcp_sock *tsk) {
    pthread_mutex_lock(&timer_lock);
    list_delete_entry(&tsk->retrans_timer.list);
    tsk->ref_cnt -= 1;
    pthread_mutex_unlock(&timer_lock);
}
