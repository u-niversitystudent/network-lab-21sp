#include "my_http.h"

char *ok = "HTTP/1.1 200 OK\r\nContent-Length: ";
char *fail = "HTTP/1.1 404 File Not Found\r\n\r\n";

int main(int argc, const char *argv[]) {
  // Port: input/default
  int port = (argc < 2) ? SPORT : atoi(argv[1]);

  // [Both] create socket file descriptor
  int s;
  struct sockaddr_in server, client;

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Create socket failed");
    return -1;
  }
  printf("Socket created\n");

  // set reuseaddr [might be useful in other projects]
  int yes = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    perror("Setsockopt error: ");
    return -1;
  }

  // Bind socket fd with monitor address
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Bind failed");
    return -1;
  }
  printf("Bind done\n");

  // Start listen
  listen(s, MAX_SESSION);
  printf("Waiting for incoming connections...\n");

  // Accept connection from an incoming client
  while (1) {
    pthread_t thread;
    int *cs;
    cs = (int *)malloc(sizeof(int));

    int c = sizeof(struct sockaddr_in);
    if ((*cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
      perror("Accept failed");
      return -1;
    }
    printf("Connection from %s was accepted\n", inet_ntoa(client.sin_addr));
    // 1 thread for 1 connection
    if (pthread_create(&thread, NULL, receiver, cs) != 0) {
      perror("pthread_create failed: ");
      return -1;
    }
  }
  return 0;
}

// single thread work for respons
void *receiver(void *cs) {
  char clt_req[MSG_SZ], ans[MSG_SZ], file[MSG_SZ], dpath[NAME_SZ];
  int len = 0;
  int sock = *((int *)cs);

  while (1) {
    // Clear buffer
    memset(clt_req, 0, MSG_SZ);
    memset(ans, 0, MSG_SZ);
    memset(file, 0, MSG_SZ);
    memset(dpath, 0, NAME_SZ);

    // RECV req; SEND ans & data
    len = recv(sock, clt_req, sizeof(clt_req), 0);
    if (len > 0) /* recv ok */ {
      // get dpath from req
      sscanf(clt_req, "%*s /%[^ ]", dpath);
      printf("%s\n", dpath);

      // Create a fp for dpath
      FILE *fp = fopen(dpath, "r");

      // If file exists
      if (fp) /* 200 OK */ {
        printf("Start sending %s to the client.\n", dpath);
        unsigned long rret = 0;
        while ((rret = fread(file, sizeof(char), MSG_SZ, fp)) > 0) {
          sprintf(ans, "%s%lu\r\n\r\n%s", ok, rret, file);
          printf("%s\n", ans);
          if (send(sock, ans, strlen(ans), 0) < 0) {
            printf("File send failed\n");
            break;
          }
        }
        fclose(fp);
        printf("File transfer finished\n");
      } else /* 404 NOT FOUND*/ {
        printf("File %s not found\n", dpath);
        strcpy(ans, fail);
        if (send(sock, ans, strlen(ans), 0) < 0) {
          printf("Answer send failed\n");
          break;
        }
      }
    } else /* exit or error */ {
      if (len == 0) // disconnected (exit normally as designed)
        printf("Client disconnected\n");
      else // recv failed (exit with error)
        perror("Recv failed: ");

      // close socket
      close(sock);
      free(cs);
      // exit the thread
      pthread_exit(NULL);
    }
  }
  return NULL;
}
