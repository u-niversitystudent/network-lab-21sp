#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

const char *in_if_str = "internal-iface:";
const char *ex_if_str = "external-iface:";
const char *dnat_str = "dnat-rules:";

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name) {
    iface_info_t *iface = NULL;
    list_for_each_entry(iface, &instance->iface_list, list) {
        if (strcmp(iface->name, if_name) == 0)
            return iface;
    }

    log(ERROR, "Could not find the desired interface according to if_name '%s'",
        if_name);
    return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet) {
    // OK: determine the direction of this packet.
    struct iphdr *ih = packet_to_ip_hdr(packet);
    rt_entry_t *rtRes = longest_prefix_match(ntohl(ih->saddr));

    if (rtRes->iface->index == nat.internal_iface->index)
        return DIR_OUT;
    else if (rtRes->iface->index == nat.external_iface->index)
        return DIR_IN;
    else return DIR_INVALID;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir) {
    // OK: do translation for this packet

    // get remote info via dir and pkt_hdr
    struct iphdr *ih = packet_to_ip_hdr(packet);
    u32 dAddr = ntohl(ih->daddr);
    u32 sAddr = ntohl(ih->saddr);
    u32 rmtAddr = (dir == DIR_IN) ? sAddr : dAddr;

    struct tcphdr *th = packet_to_tcp_hdr(packet);
    u16 sPort = ntohs(th->sport);
    u16 dPort = ntohs(th->dport);
    u16 rmtPort = (dir == DIR_IN) ? sPort : dPort;

    // get index by hash8
    char buf[6] = {0};
    strncat(buf, (char *) &rmtAddr, 4);
    strncat(buf + 4, (char *) &rmtPort, 2);
    u8 index = hash8(buf, 6);
#ifdef DEBUG_HASH
    printf("buf = %u%hu; hash index = %d\n", rmtAddr, rmtPort, index);
#endif
    struct list_head *nm_list = &nat.nat_mapping_list[index];
    pthread_mutex_lock(&nat.lock);
    time_t now = time(NULL);

    /* Already have connection */
    struct nat_mapping *pos_nm;
    list_for_each_entry(pos_nm, nm_list, list) {
        // if not the mapping we search, skip
        if (rmtAddr != pos_nm->remote_ip ||
            rmtPort != pos_nm->remote_port)
            continue;

        if (dir == DIR_IN) {
            if (dAddr != pos_nm->external_ip ||
                dPort != pos_nm->external_port)
                continue;

            ih->daddr = htonl(pos_nm->internal_ip);
            th->dport = htons(pos_nm->internal_port);
            pos_nm->conn.external_fin =
                    (th->flags & TCP_FIN) ? TCP_FIN : 0;
            pos_nm->conn.external_seq_end = tcp_seq_end(ih, th);
            // if ack
            if (th->flags & TCP_ACK)
                pos_nm->conn.external_seq_end = th->ack;
        } else {
            if (sAddr != pos_nm->internal_ip ||
                sPort != pos_nm->internal_port)
                continue;


            ih->saddr = htonl(pos_nm->external_ip);
            th->sport = htons(pos_nm->external_port);
            pos_nm->conn.internal_fin =
                    (th->flags & TCP_FIN) ? TCP_FIN : 0;
            pos_nm->conn.internal_seq_end = tcp_seq_end(ih, th);
            // if ack
            if (th->flags & TCP_ACK)
                pos_nm->conn.internal_ack = th->ack;
        }
        // packed and send
        pos_nm->update_time = now;
        th->checksum = tcp_checksum(ih, th);
        ih->checksum = ip_checksum(ih);
        ip_send_packet(packet, len);

        pthread_mutex_unlock(&nat.lock);
        return;
    }

    /* No connection yet */

    // Not DNAT/SNAT => INVALID
    if ((th->flags & TCP_SYN) == 0) {
        fprintf(stdout, "INVALID PACKET\n");
        icmp_send_packet(packet, len,
                         ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
        free(packet);

        pthread_mutex_unlock(&nat.lock);
        return;
    }

    // Build new connection
    if (dir == DIR_OUT) {
        u16 pid;
        for (pid = NAT_PORT_MIN; pid <= NAT_PORT_MAX; ++pid) {
            if (!nat.assigned_ports[pid]) {
                nat.assigned_ports[pid] = 1;
                struct nat_mapping *new_nm =
                        (struct nat_mapping *) malloc(
                                sizeof(struct nat_mapping));
                list_add_tail(&new_nm->list, nm_list);

                new_nm->update_time = now;
                new_nm->remote_ip = rmtAddr;
                new_nm->remote_port = rmtPort;
                new_nm->external_ip = nat.external_iface->ip;
                new_nm->external_port = pid;
                new_nm->internal_ip = sAddr;
                new_nm->internal_port = sPort;
                new_nm->conn.internal_fin =
                        (th->flags & TCP_FIN) ? TCP_FIN : 0;
                new_nm->conn.internal_seq_end = tcp_seq_end(ih, th);
                if (th->flags & TCP_ACK)
                    new_nm->conn.internal_ack = th->ack;

                ih->saddr = htonl(new_nm->external_ip);
                th->sport = htons(new_nm->external_port);
                th->checksum = tcp_checksum(ih, th);
                ih->checksum = ip_checksum(ih);

                ip_send_packet(packet, len);

                pthread_mutex_unlock(&nat.lock);
                return;
            }
        }
    } else {
        struct dnat_rule *posRule;
        list_for_each_entry(posRule, &nat.rules, list) {
            if (dAddr == posRule->external_ip &&
                dPort == posRule->external_port) {
                struct nat_mapping *new_nm =
                        (struct nat_mapping *) malloc(
                                sizeof(struct nat_mapping));
                list_add_tail(&new_nm->list, nm_list);

                new_nm->update_time = now;
                new_nm->remote_ip = rmtAddr;
                new_nm->remote_port = rmtPort;
                new_nm->external_ip = posRule->external_ip;
                new_nm->external_port = posRule->external_port;
                new_nm->internal_ip = posRule->internal_ip;
                new_nm->internal_port = posRule->internal_port;
                new_nm->conn.external_fin =
                        (th->flags & TCP_FIN) ? TCP_FIN : 0;
                new_nm->conn.external_seq_end = tcp_seq_end(ih, th);
                if (th->flags & TCP_ACK)
                    new_nm->conn.external_ack = th->ack;

                ih->daddr = htonl(posRule->internal_ip);
                th->dport = htons(posRule->internal_port);
                th->checksum = tcp_checksum(ih, th);
                ih->checksum = ip_checksum(ih);
                ip_send_packet(packet, len);

                pthread_mutex_unlock(&nat.lock);
                return;
            }
        }
    }

    // CANNOT ALLOCATE PORT
    fprintf(stdout, "NAT OVERLOAD: NO AVAILABLE PORT\n");
    icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
    free(packet);

    pthread_mutex_unlock(&nat.lock);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len) {
    int dir = get_packet_direction(packet);
    if (dir == DIR_INVALID) {
        log(ERROR, "invalid packet direction, drop it.");
        icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
        free(packet);
        return;
    }

    struct iphdr *ip = packet_to_ip_hdr(packet);
    if (ip->protocol != IPPROTO_TCP) {
        log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
        free(packet);
        return;
    }

    do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn) {
    return (conn->internal_fin && conn->external_fin) &&
           (conn->internal_ack >= conn->external_seq_end) &&
           (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout() {
    while (1) {
        // OK: sweep finished flows periodically

        pthread_mutex_lock(&nat.lock);

        time_t now = time(NULL);
        for (int i = 0; i < HASH_8BITS; ++i) {
            struct nat_mapping *pos_nm, *q_nm;
            list_for_each_entry_safe(
                    pos_nm, q_nm, &nat.nat_mapping_list[i], list) {
                if (now - pos_nm->update_time > TCP_ESTABLISHED_TIMEOUT
                    || is_flow_finished(&pos_nm->conn)) {
                    nat.assigned_ports[pos_nm->external_port] = 0;
                    list_delete_entry(&pos_nm->list);
                    free(pos_nm);
                }
            }
        }

        pthread_mutex_unlock(&nat.lock);

        sleep(1);
    }

    return NULL;
}

int parse_config(const char *filename) {
    // OK: parse config file, including i-iface, e-iface
    // (and dnat-rules if existing)

    // open file
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL) {
        perror("Error(s) occur when accessing data");
        exit(1);
    }
    // single line buffer
    char line[MAX_LINE_LEN], in,
            partA[BUF_LEN], partB[BUF_LEN],
            dnat_ex[BUF_LEN], dnat_in[BUF_LEN];
    // reading
    int loc, flag_eof = 0;
    while (flag_eof == 0) {
        // flush buffer
        loc = 0;
        memset(line, 0, MAX_LINE_LEN);
        memset(partA, 0, BUF_LEN);
        memset(partB, 0, BUF_LEN);
        memset(dnat_in, 0, BUF_LEN);
        memset(dnat_ex, 0, BUF_LEN);

        // get content of current line
        while ((in = fgetc(fptr)) != '\n') {
            if (in == EOF) {
                flag_eof = 1;
                break;
            } else line[loc++] = in;
        }
        // select parsing method and GO!
        if (loc == 0) continue;
#ifdef DEBUG_PARSE
        printf("%s\n", line);
#endif
        if (strncmp(dnat_str, line, strlen(dnat_str)) == 0) {
            // dnat newRule
            // sample:
            // dnat-rules: 159.226.39.43:8001 -> 10.21.0.2:8000
            struct dnat_rule *newRule =
                    (struct dnat_rule *) malloc(sizeof(struct dnat_rule));
            list_add_tail(&newRule->list, &nat.rules);

            sscanf(line, "%s %s %s %s",
                   partA, dnat_ex, partB, dnat_in);
            u32 ip_ex, ip_in;
            u16 port_ex, port_in;
            u32 ip4, ip3, ip2, ip1;
            sscanf(dnat_ex, "%[^:]:%s", partA, partB);
            port_ex = atoi(partB);
            sscanf(partA, "%u.%u.%u.%u", &ip4, &ip3, &ip2, &ip1);
            ip_ex = (ip4 << 24) | (ip3 << 16) | (ip2 << 8) | (ip1);
            newRule->external_ip = ip_ex;
            newRule->external_port = port_ex;
            fprintf(stdout, "   |---[EX] IP: "IP_FMT" PORT: %hu\n",
                    HOST_IP_FMT_STR(ip_ex), port_ex);

            sscanf(dnat_in, "%[^:]:%s", partA, partB);
            port_in = atoi(partB);
            sscanf(partA, "%u.%u.%u.%u", &ip4, &ip3, &ip2, &ip1);
            ip_in = (ip4 << 24) | (ip3 << 16) | (ip2 << 8) | (ip1);
            newRule->internal_ip = ip_in;
            newRule->internal_port = port_in;
            fprintf(stdout, "   |---[IN] IP: "IP_FMT" PORT: %hu\n",
                    HOST_IP_FMT_STR(ip_in), port_in);
        } else {
            sscanf(line, "%s %s", partA, partB);
            // sample:
            // dnat_in-iface: n1-eth0
            if (strncmp(in_if_str, partA, strlen(in_if_str)) == 0) {
                // in iface
                nat.internal_iface = if_name_to_iface(partB);
                fprintf(stdout, "[Load %s]: %s.\n", partA, partB);
            } else if (strncmp(ex_if_str, partA, strlen(ex_if_str))
                       == 0) {
                // ex iface
                nat.external_iface = if_name_to_iface(partB);
                fprintf(stdout, "[Load %s]: %s.\n", partA, partB);
            }
        }
    }

    // exit normally
    fclose(fptr);
    return 0;
}

// initialize
void nat_init(const char *config_file) {
    memset(&nat, 0, sizeof(nat));

    for (int i = 0; i < HASH_8BITS; i++)
        init_list_head(&nat.nat_mapping_list[i]);

    init_list_head(&nat.rules);

    // seems unnecessary
    memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

    parse_config(config_file);

    pthread_mutex_init(&nat.lock, NULL);

    pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit() {
    // OK: release all resources allocated
    pthread_mutex_lock(&nat.lock);
    for (int i = 0; i < HASH_8BITS; ++i) {
        struct nat_mapping *pos_nm, *q_nm;
        list_for_each_entry_safe(pos_nm, q_nm,
                                 &nat.nat_mapping_list[i], list) {
            list_delete_entry(&pos_nm->list);
            free(pos_nm);
        }
    }
    pthread_mutex_unlock(&nat.lock);

    pthread_kill(nat.thread, SIGKILL);
}
