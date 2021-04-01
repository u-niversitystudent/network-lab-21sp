#include "my_http.h"

/*
HTTP GET REQUEST FORMAT (WITHOUT REG-DATA)
   [REQ LINE]  : |Req-method| |dpath| |Protocol-version|\r\n|
   [REQ HEAD]  : |Head-segname:|value|\r\n|
                 (e.g. HOST:http://10.0.0.1:80)
   [BLANK LINE]: |\r\n|
*/

char *method = "GET";    // method
char *ptcl = "HTTP/1.1"; // protocol
char *hseg = "HOST:";    // head segname+":"
char *path_out = "out/"; // output path

int main(int argc, char *argv[]) {

  char usr_req[MSG_SZ], svr_ans[MSG_SZ];

  char url[NAME_SZ * 2], dhost[NAME_SZ], dpath[NAME_SZ];
  memset(url, 0, NAME_SZ * 2);
  int f_in = 0;

  if (argc > 2)
    perror("Too many args");
  else if (argc > 1) {
    f_in = 1; // If input a dest, fin = 1
    memcpy(url, argv[1], (strlen(argv[1]) + 1));
  }

  // SEND-RECV
  while (1) {

    // Clear buffer
    memset(usr_req, 0, MSG_SZ);
    memset(svr_ans, 0, MSG_SZ);

    // If input argv[1] not used
    if (!f_in) {
      printf("Enter url (host+path): (type 'q' to quit)\n");
      scanf("%s", url);
      if (strcmp(url, "q") == 0)
        break;
    } else
      f_in = 0;

    // Get dhost+dpath from url
    decode_input(url, dhost, dpath);

    // Create a GET Request
    sprintf(usr_req, "%s %s %s\r\n%s%s\r\n\r\n", method, dpath, ptcl, hseg,
            url);

    // [Both] Create socket file descriptor
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
      printf("SOCKET FAILED\n");
      return -1;
    }
    printf("SOCKET CREATED\n");

    // Connect to server
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(dhost);
    server.sin_family = AF_INET;
    server.sin_port = htons(SPORT);
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
      perror("CONNECT FAILED: ");
      return 1;
    }
    printf("CONNECTED!!!\n");

    if (send(sock, usr_req, strlen(usr_req), 0) < 0) {
      printf("SEND FAILED\n");
      return 1;
    }
    printf("SENT!!!\n");

    // Get destination string
    char dest[NAME_SZ];
    memset(dest, 0, NAME_SZ);
    strcat(dest, path_out);
    strcat(dest, dpath);

    // Get ans
    int len_ans = 0;
    memset(svr_ans, 0, sizeof(svr_ans));
    while ((len_ans = recv(sock, svr_ans, MSG_SZ, 0)) != 0) {
      if (len_ans < 0) {
        printf("RECV FAILED!\n");
        break;
      }
      // Print answer from server
      printf("%s\n", svr_ans);

      // decode state in ans
      char state[4];
      sscanf(svr_ans, "%*[^ ] %[^ ]", state);
      if (strcmp(state, "404") != 0) {
        printf("%s OK!\n", state);
        // skip head
        int start;
        for (start = 0; start < len_ans; start++) {
          if (svr_ans[start] == '\r' && svr_ans[start + 1] == '\n' &&
              svr_ans[start + 2] == '\r' && svr_ans[start + 3] == '\n') {
            start += 4;
            break;
          }
        }
        // save file
        FILE *fp = fopen(dest, "a+");
        unsigned long wret =
            fwrite(&svr_ans[start], sizeof(char), len_ans - start, fp);
        if (wret < len_ans - start) {
          printf("FWRITE FAILED!\n");
          break;
        }
        printf("FWRITE FINISHED!\n");
        fclose(fp);
        break;
      } else {
        printf("404 File Not Found.\n");
        break;
      }
    }
    // [Both] close socket
    printf("EXIT...\n");
    close(sock);
  }

  return 0;
}

// decode host & path from url
void decode_input(char *url, char *dhost, char *dpath) {
  int num_slash = 0, len_http = 0, len_dhost = 0, len_dpath = 0;

  memset(dhost, 0, NAME_SZ);
  memset(dpath, 0, NAME_SZ);

  int len_url = strlen(url) + 1;

  for (int i = 0; i < len_url; i++) {
    char c = *(url + i);

    if (num_slash < 2) { // http-prefix "http://"
      num_slash += (c == '/');
      len_http++;
      continue;
    }

    num_slash += (c == '/');
    if (num_slash == 2) { // dhost "10.0.0.1"
      *(dhost + i - len_http) = c;
      len_dhost++;
    } else { // dpath "/test.html"
      *(dpath + i - len_http - len_dhost) = c;
      len_dpath++;
    }
  }
}