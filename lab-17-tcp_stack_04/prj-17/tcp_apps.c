#include "tcp_sock.h"
#include "ip.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>

//#define STRING_DELIVERY

#ifndef STRING_DELIVERY
#define DAT_CLIENT "client-input.dat"
#define DAT_SERVER "server-output.dat"
#endif

// tcp server application, listens to port (specified by arg)
// and serves only one connection request
void *tcp_server(void *arg) {
    u16 port = *(u16 *) arg;
    struct tcp_sock *tsk = alloc_tcp_sock();

    struct sock_addr addr;
    addr.ip = htonl(0);
    addr.port = port;
    if (tcp_sock_bind(tsk, &addr) < 0) {
        log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
        exit(1);
    }

    if (tcp_sock_listen(tsk, 3) < 0) {
        log(ERROR, "tcp_sock listen failed");
        exit(1);
    }

    log(DEBUG, "listen to port %hu.", ntohs(port));

    struct tcp_sock *csk = tcp_sock_accept(tsk);

    log(DEBUG, "accept a connection.");

    char rbuf[1001];
    int rlen = 0;
#ifndef STRING_DELIVERY
    fprintf(stdout, "Start receiving file %s from client...\n",
            DAT_SERVER);
    FILE *fp = fopen(DAT_SERVER, "w+");
    u32 wlen = 0;
    while (1) {
        memset(rbuf, 0, sizeof(rbuf));
        rlen = tcp_sock_read(csk, rbuf, 1000);
        if (rlen == 0) {
            log(DEBUG, "tcp_sock_read return 0, finish transmission.");
            break;
        } else if (rlen > 0) {
            wlen = fwrite(rbuf, sizeof(char), rlen, fp);
            fflush(fp);
            if (wlen < rlen) {
                fprintf(stdout, "Mistakes during file writing.\n");
                break;
            }
        } else {
            log(DEBUG,
                "tcp_sock_read return negative value, something goes wrong.");
            exit(1);
        }
    }
    usleep(100);
    fclose(fp);
#else
    char wbuf[1024];
    while (1) {
        rlen = tcp_sock_read(csk, rbuf, 1000);
        if (rlen == 0) {
            log(DEBUG, "tcp_sock_read return 0, finish transmission.");
            break;
        } else if (rlen > 0) {
            rbuf[rlen] = '\0';
            sprintf(wbuf, "server echoes: %s", rbuf);
            if (tcp_sock_write(csk, wbuf, strlen(wbuf)) < 0) {
                log(DEBUG,
                    "tcp_sock_write return negative value, something goes wrong.");
                exit(1);
            }
        } else {
            log(DEBUG,
                "tcp_sock_read return negative value, something goes wrong.");
            exit(1);
        }
    }
#endif
    log(DEBUG, "close this connection.");

    tcp_sock_close(csk);

    return NULL;
}

// tcp client application,
// connects to server (ip:port specified by arg),
// each time sends one bulk of data
// and receives one bulk of data
void *tcp_client(void *arg) {
    struct sock_addr *skaddr = arg;

    struct tcp_sock *tsk = alloc_tcp_sock();

    if (tcp_sock_connect(tsk, skaddr) < 0) {
        log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
                NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
        exit(1);
    }

    char rbuf[1001];
    int dlen;

#ifndef STRING_DELIVERY
    FILE *fp = fopen(DAT_CLIENT, "r");
    if (fp) {
        fprintf(stdout, "Start sending file %s to the server...\n",
                DAT_CLIENT);
        memset(rbuf, 0, sizeof(rbuf));
        while ((dlen = fread(rbuf, sizeof(char), 1000, fp)) > 0) {
            if (tcp_sock_write(tsk, rbuf, dlen) < 0) {
                fprintf(stdout, "Error encounter when write to buf\n");
                break;
            }
            memset(rbuf, 0, sizeof(rbuf));
            if (feof(fp)) break;
            // usleep(100);
        }
        fclose(fp);
        fprintf(stdout, "Finish sending process.\n");
    } else {
        fprintf(stdout, "File %s not found.\n", DAT_CLIENT);
    }
    // sleep(1);
#else
    int rlen = 0;
    char *data = "0123456789abcdefghijklmnopqrstuvw"
                 "xyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    dlen = strlen(data);
    char *wbuf = malloc(dlen + 1);

    int n = 10;
    for (int i = 0; i < n; i++) {
        memcpy(wbuf, data + i, dlen - i);
        if (i > 0) memcpy(wbuf + (dlen - i), data, i);

        if (tcp_sock_write(tsk, wbuf, dlen) < 0)
            break;

        rlen = tcp_sock_read(tsk, rbuf, 1000);
        if (rlen == 0) {
            log(DEBUG, "tcp_sock_read return 0, finish transmission.");
            break;
        } else if (rlen > 0) {
            rbuf[rlen] = '\0';
            fprintf(stdout, "%s\n", rbuf);
        } else {
            log(DEBUG,
                "tcp_sock_read return negative value, something goes wrong.");
            exit(1);
        }
        sleep(1);
    }

    free(wbuf);
#endif

    tcp_sock_close(tsk);

    return NULL;
}
