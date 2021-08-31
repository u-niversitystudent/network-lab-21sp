#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
#include <sys/time.h>

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero,
// notify tcp_sock_send (wait_send)
static inline void tcp_update_window(
        struct tcp_sock *tsk, struct tcp_cb *cb) {
    u16 old_snd_wnd = tsk->snd_wnd;
    tsk->adv_wnd = cb->rwnd;
    tsk->snd_wnd = min(tsk->adv_wnd, tsk->cwnd * (MTU_SIZE));
    if (old_snd_wnd == 0)
        wake_up(tsk->wait_send);
}

static inline void tcp_cwnd_inc(struct tcp_sock *tsk) {
    if (tsk->cgt_state != OPEN) return;
    if (tsk->cwnd < tsk->ssthresh) {
        tsk->cwnd += 1;
    } else {
        tsk->cwnd_unit += 1;
        if (tsk->cwnd_unit >= tsk->cwnd) {
            tsk->cwnd_unit = 0;
            tsk->cwnd += 1;
        }
    }
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


static int
tcp_recv_psh_ack_tool(struct tcp_sock *tsk, struct tcp_cb *cb) {
    pthread_mutex_lock(&tsk->rcv_buf->rbuf_lock);
    u32 seq_end = tsk->rcv_nxt;
    if (seq_end > cb->seq) {
        // OK: consider control packet loss
        pthread_mutex_unlock(&tsk->rcv_buf->rbuf_lock);
        tcp_send_control_packet(tsk, TCP_ACK);
        return 1;
    } else if (seq_end == cb->seq) {
        write_ring_buffer(tsk->rcv_buf,
                          cb->payload, cb->pl_len);
        seq_end = cb->seq_end;
        struct ofo_buffer *pos_ofo, *q_ofo;
        list_for_each_entry_safe(pos_ofo, q_ofo,
                                 &tsk->rcv_ofo_buf,
                                 list) {
            if (seq_end < pos_ofo->seq) break;

            seq_end = pos_ofo->seq_end;
            write_ring_buffer(pos_ofo->tsk->rcv_buf,
                              pos_ofo->payload,
                              pos_ofo->pl_len);
            list_delete_entry(&pos_ofo->list);
            free(pos_ofo->payload);
            free(pos_ofo);
        }
        tsk->rcv_nxt = seq_end;
    } else if (seq_end < cb->seq) {
        WriteOfoBuf(tsk, cb);
    }
    pthread_mutex_unlock(&tsk->rcv_buf->rbuf_lock);

    if (tsk->wait_recv->sleep) wake_up(tsk->wait_recv);
    tcp_send_control_packet(tsk, TCP_ACK);
    if (tsk->wait_send->sleep) wake_up(tsk->wait_send);

    return 0;
}

// Process the incoming packet according to TCP state machine.
void tcp_process(
        struct tcp_sock *tsk, struct tcp_cb *cb, char *packet) {

    if (!tsk) {
        fprintf(stdout, "No tsk record.\n");
        return;
    }

    if ((cb->flags & TCP_PSH) == 0) {
        tsk->rcv_nxt = cb->seq_end;
    }

    if ((cb->flags) & TCP_ACK) {
        if (cb->ack == tsk->snd_una) {
            tsk->rep_ack++;
            switch (tsk->cgt_state) {
                case OPEN:
                    if (tsk->rep_ack > 2) {
                        tsk->cgt_state = RCVR;
                        tsk->ssthresh = (tsk->cwnd + 1) / 2;
                        tsk->recovery_point = tsk->snd_nxt;
                        struct send_buffer *buf = NULL;
                        list_for_each_entry(
                                buf, &tsk->send_buf, list)break;
                        if (!list_empty(&tsk->send_buf)) {
                            char *temp = (char *) malloc(
                                    buf->len * sizeof(char));
                            memcpy(temp, buf->packet, buf->len);
                            ip_send_packet(temp, buf->len);
                        }
                    } else break;
                case RCVR:
                    if (tsk->rep_ack > 1) {
                        tsk->rep_ack -= 2;
                        tsk->cwnd -= 1;
                        if (tsk->cwnd < 1) tsk->cwnd = 1;
                    }
                    break;
                default:
                    break;
            }
        } else {
            tsk->rep_ack = 0;
            struct send_buffer *buf, *q;
            list_for_each_entry_safe(buf, q,
                                     &tsk->send_buf, list) {
                if (buf->seq_end > cb->ack) break;
                else {
                    tsk->snd_una = buf->seq_end;
                    PopSendBuf(tsk, buf);
                    tcp_cwnd_inc(tsk);
                }
            }
            switch (tsk->cgt_state) {
                case RCVR:
                    if (less_or_equal_32b(
                            tsk->recovery_point, cb->ack)) {
                        tsk->cgt_state = OPEN;
                    } else {
                        char *temp =
                                (char *) malloc(
                                        buf->len * sizeof(char));
                        memcpy(temp, buf->packet, buf->len);
                        ip_send_packet(temp, buf->len);
                    }
                    break;
                case LOSS:
                    if (less_or_equal_32b(
                            tsk->recovery_point, cb->ack)) {
                        tsk->cgt_state = OPEN;
                    }
                    break;
                default:
                    break;
            }
        }
        tcp_update_window_safe(tsk, cb);
    }


    if (cb->flags & TCP_ACK) {
        struct send_buffer *pos_buf, *q_buf;
        list_for_each_entry_safe(pos_buf, q_buf,
                                 &tsk->send_buf, list) {

            if (pos_buf->seq_end > cb->ack) break;
            else {
                tsk->snd_una = pos_buf->seq_end;
                PopSendBuf(tsk, pos_buf);
            }
        }
    }

    // tsk->snd_una = cb->ack;
    // tsk->rcv_nxt = cb->seq_end;

    // FSM that refers to sock status
    switch (tsk->state) {
        case TCP_CLOSED:
            break;
        case TCP_LISTEN:
            if (cb->flags & TCP_SYN) {
                // XXX:
                // the process of recv a new SYN should be
                // checked carefully, especially when you
                // alloc new sock and assign values to certain areas
                struct tcp_sock *alc_tsk = alloc_tcp_sock();
                alc_tsk->parent = tsk;
                alc_tsk->sk_sip = cb->daddr;
                alc_tsk->sk_sport = cb->dport;
                alc_tsk->sk_dip = cb->saddr;
                alc_tsk->sk_dport = cb->sport;
                alc_tsk->iss = tcp_new_iss();
                alc_tsk->snd_nxt = alc_tsk->iss;
                alc_tsk->rcv_nxt = tsk->rcv_nxt;
                alc_tsk->snd_una = tsk->snd_una;

                struct sock_addr *pSockAddr =
                        (struct sock_addr *) malloc(
                                sizeof(struct sock_addr));
                pSockAddr->ip = htonl(cb->daddr);
                pSockAddr->port = htons(cb->dport);
                // tcp_sock_bind(alc_tsk, pSockAddr);
                // tcp_hash(alc_tsk);
                // finish the assignment and add it to listen queue
                // list_add_tail(&alc_tsk->list, &tsk->listen_queue);

                tcp_set_state(alc_tsk, TCP_SYN_RECV);
                tcp_hash(alc_tsk);
                tcp_send_control_packet(alc_tsk,
                                        TCP_SYN | TCP_ACK);
            }
            break;
        case TCP_SYN_RECV:
            if (cb->flags & TCP_ACK) {
                if (cb->flags & TCP_PSH) {
                    if (tsk->cgt_state == OPEN &&
                        tsk->wait_send->sleep) {
                        wake_up(tsk->wait_send);
                    }
                    tcp_set_state(tsk, TCP_ESTABLISHED);
                    // OK: write data
                    // if (tcp_recv_psh_ack_tool(tsk, cb)) break;
                    tcp_recv_psh_ack_tool(tsk, cb);
                } else {
                    tcp_sock_accept_enqueue(tsk);
                    wake_up(tsk->parent->wait_accept);
                    tcp_set_state(tsk, TCP_ESTABLISHED);
                }
            }
            break;
        case TCP_SYN_SENT:
            if ((cb->flags & TCP_ACK) &&
                (cb->flags & TCP_SYN)) {
                wake_up(tsk->wait_connect);
            }
            break;
        case TCP_ESTABLISHED:
            if (cb->flags & TCP_FIN) {
                tcp_set_state(tsk, TCP_LAST_ACK);
                tcp_send_control_packet(tsk, TCP_ACK | TCP_FIN);
                tcp_set_timewait_timer(tsk);
            } else if (cb->flags & TCP_ACK) {
                if (tsk->cgt_state == OPEN)
                    wake_up(tsk->wait_send);
                if (cb->flags & TCP_PSH) {
                    if (tsk->cgt_state == OPEN &&
                        tsk->wait_send->sleep) {
                        wake_up(tsk->wait_send);
                    }
                    tcp_recv_psh_ack_tool(tsk, cb);
                } else {
                    wake_up(tsk->wait_send);
                }
            }
            break;
        case TCP_CLOSE_WAIT:
            break;
        case TCP_LAST_ACK:
            if (cb->flags & TCP_ACK) {
                tcp_set_state(tsk, TCP_CLOSED);
                if (!tsk->parent) tcp_bind_unhash(tsk);
                tcp_unhash(tsk);
            }
            break;
        case TCP_FIN_WAIT_1:
            if (cb->flags & TCP_ACK) {
                if (cb->flags & TCP_FIN) {
                    tcp_set_state(tsk, TCP_TIME_WAIT);
                    tcp_send_control_packet(tsk, TCP_ACK);
                    tcp_set_timewait_timer(tsk);
                } else {
                    tcp_set_state(tsk, TCP_FIN_WAIT_2);
                }
            }
            break;
        case TCP_FIN_WAIT_2:
            if (cb->flags & TCP_FIN) {
                tcp_set_state(tsk, TCP_TIME_WAIT);
                tcp_send_control_packet(tsk, TCP_ACK);
                tcp_set_timewait_timer(tsk);
            }
            break;
        case TCP_CLOSING:
            break;
        case TCP_TIME_WAIT:
            break;
        default:
            if (cb->flags & TCP_FIN) {
                fprintf(stdout,
                        "  [flag=FIN] No rule for %d\n",
                        tsk->state);
            } else if (cb->flags & TCP_SYN) {
                fprintf(stdout, "Recv SYN when state=%d\n", tsk->state);
            } else if (cb->flags & TCP_ACK) {
                if (cb->flags & TCP_PSH) {
                    fprintf(stdout,
                            "  [flag=ACK|PSH] No rule for %d\n",
                            tsk->state);
                } else if (cb->flags & TCP_FIN) {
                    fprintf(stdout,
                            "  [flag=ACK|FIN] No rule for %d\n",
                            tsk->state);
                } else {
                    fprintf(stdout, "  [flag=ACK] No rule for %d\n",
                            tsk->state);
                }
            }
            break;
    }

}
