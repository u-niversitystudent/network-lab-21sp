//
// Created by zheng on 5/17/2021.
//

#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "fmt.h"
#include "string.h"
#include "general.h"

void read_all_data(FILE *fptr, char *path, u32 *ip, u32 *mask, u32 *port) {

    // open file
    fptr = fopen(path, "r");
    if (fptr == NULL) {
        perror("Error occur when accessing data");
        exit(1);
    }

    // single line buffer
    char line[MAX_LINE_LEN] = {0}, in;
    // test
    int loc, flag_eof = 0;
    for (int i = 0; i < NUM_REC && flag_eof == 0; ++i) {
        // flush buffer
        loc = 0;
        memset(line, 0, MAX_LINE_LEN);

        while ((in = fgetc(fptr)) != '\n') {
            if (in == EOF) {
                flag_eof = 1;
                break;
            } else line[loc++] = in;
        }

        ip[i] = ip_str_to_u32(line);
        mask[i] = mask_str_to_u32(line);
        port[i] = port_str_to_u32(line);
    }

    // exit normally
    fclose(fptr);
}
