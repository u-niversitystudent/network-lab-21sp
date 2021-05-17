//
// Created by zheng on 5/17/2021.
//

#include <stdio.h>

#include "ip.h"
#include "types.h"
#include "fmt.h"


u32 ip_str_to_u32(char *input) {
    u32 buf = 0;
    u32 ip = 0;
    for (int i = 0; i < strlen(input); ++i) {
        char c = *(input + i);
        if (IF_SPLIT(c)) {
            ip = buf + (ip << 8);
            break;
        } else if (IF_NUM(c)) {
            buf = buf * 10 + TO_NUM(c);
        } else if (IF_DOT(c)) {
            ip = buf + (ip << 8);
            buf = 0;
        } else {
            printf("Maybe Bug here...\n");
            exit(-1);
        }
    }
    return ip;
}

u32 mask_str_to_u32(char *input) {
    u32 mask = 0;
    int count_split = 0;
    for (int i = 0; i < strlen(input); ++i) {
        char c = *(input + i);
        if (IF_SPLIT(c)) {
            count_split++;
            continue;
        }
        if (count_split == 1)
            if (IF_NUM(c)) mask = (mask * 10) + TO_NUM(c);
    }
    return mask;
}

u32 port_str_to_u32(char *input) {
    u32 port = 0;
    int count_split = 0;
    for (int i = 0; i < strlen(input); ++i) {
        char c = *(input + i);
        if (IF_SPLIT(c)) {
            count_split++;
            continue;
        }
        if (count_split == 2)
            if (IF_NUM(c)) port = (port * 10) + TO_NUM(c);
    }
    return port;
}