#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero,
// notify tcp_sock_send (wait_send)
static inline void tcp_update_window(
        struct tcp_sock *tsk, struct tcp_cb *cb) {
    u16 old_snd_wnd = tsk->snd_wnd;
    tsk->snd_wnd = cb->rwnd;
    if (old_snd_wnd == 0)
        wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should
// be between snd_una and snd_nxt
static inline void
tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb) {
    if (less_or_equal_32b(tsk->snd_una, cb->ack) &&
        less_or_equal_32b(cb->ack, tsk->snd_nxt))
        tcp_update_window(tsk, cb);
}

#ifndef max
#define max(x, y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of
// the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk,
                                   struct tcp_cb *cb) {
    u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
    if (less_than_32b(cb->seq, rcv_end) &&
        less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
        return 1;
    } else {
        log(ERROR, "received packet with invalid seq, drop it.");
        return 0;
    }
}

// Process the incoming packet according to TCP state machine.
void tcp_process(
        struct tcp_sock *tsk, struct tcp_cb *cb, char *packet) {
    // OK: tcp_process in tcp_stack_01

    if (!tsk) {
        fprintf(stdout, "No tsk record.\n");
        return;
    }

    tsk->snd_una = cb->ack;
    tsk->rcv_nxt = cb->seq_end;

    switch (cb->flags) {
        case TCP_SYN:
            if (tsk->state != TCP_LISTEN) {
                fprintf(stdout, "Recv SYN when state=%d\n", tsk->state);
                break;
            }

            // XXX:
            // the process of recv a new SYN should be
            // checked carefully, especially when you
            // alloc new sock and
            // assign values to certain areas
            struct tcp_sock *alc_tsk = alloc_tcp_sock();
            memcpy((char *) alc_tsk, (char *) tsk,
                   sizeof(struct tcp_sock));
            alc_tsk->parent = tsk;
            alc_tsk->sk_sip = cb->daddr;
            alc_tsk->sk_sport = cb->dport;
            alc_tsk->sk_dip = cb->saddr;
            alc_tsk->sk_dport = cb->sport;
            alc_tsk->iss = tcp_new_iss();
            alc_tsk->snd_nxt = alc_tsk->iss;
            struct sock_addr *pSockAddr =
                    (struct sock_addr *) malloc(
                            sizeof(struct sock_addr));
            pSockAddr->ip = htonl(cb->daddr);
            pSockAddr->port = htons(cb->dport);
            tcp_sock_bind(alc_tsk, pSockAddr);
            tcp_hash(alc_tsk);
            // finish the assignment and add it to listen queue
            list_add_tail(&alc_tsk->list, &tsk->listen_queue);

            tcp_set_state(alc_tsk, TCP_SYN_RECV);
            tcp_send_control_packet(alc_tsk, TCP_SYN | TCP_ACK);
            break;
        case (TCP_SYN | TCP_ACK):
            if (tsk->state == TCP_SYN_SENT) wake_up(tsk->wait_connect);
            break;
        case TCP_ACK:
            switch (tsk->state) {
                case TCP_SYN_RECV:
                    tcp_sock_accept_dequeue(tsk);
                    wake_up(tsk->parent->wait_accept);
                    tcp_set_state(tsk, TCP_ESTABLISHED);
                    break;
                case TCP_FIN_WAIT_1:
                    tcp_set_state(tsk, TCP_FIN_WAIT_2);
                    break;
                case TCP_LAST_ACK:
                    tcp_set_state(tsk, TCP_CLOSED);
                    if (!tsk->parent) tcp_bind_unhash(tsk);
                    tcp_unhash(tsk);
                    break;
                default:
                    fprintf(stdout, "  [flag=ACK] No rule for %d\n",
                            tsk->state);
            }
            break;
        case (TCP_ACK | TCP_FIN):
            if (tsk->state != TCP_FIN_WAIT_1) {
                fprintf(stdout,
                        "  [flag=ACK|FIN] No rule for %d\n",
                        tsk->state);
                break;
            }
            tcp_set_state(tsk, TCP_TIME_WAIT);
            tcp_send_control_packet(tsk, TCP_ACK);
            tcp_set_timewait_timer(tsk);
            break;
        case TCP_FIN:
            switch (tsk->state) {
                case TCP_ESTABLISHED:
                    tcp_set_state(tsk, TCP_LAST_ACK);
                    tcp_send_control_packet(
                            tsk, TCP_ACK | TCP_FIN);
                    break;
                case TCP_FIN_WAIT_2:
                    tcp_set_state(tsk, TCP_TIME_WAIT);
                    tcp_send_control_packet(tsk, TCP_ACK);
                    tcp_set_timewait_timer(tsk);
                    break;
                default:
                    fprintf(stdout,
                            "  [flag=FIN] No rule for %d\n",
                            tsk->state);
            }
            break;
        default:
            fprintf(stdout, "No rule for flag %d\n", cb->flags);
    }
}
