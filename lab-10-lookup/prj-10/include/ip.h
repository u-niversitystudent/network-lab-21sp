//
// Created by zheng on 5/15/2021.
//

#ifndef PRJ_10_IP_H
#define PRJ_10_IP_H

#include <string.h>
#include <error.h>
#include <stdlib.h>

#include "types.h"

#define IP_LEN 32

#define IF_NUM(a) \
(((a) >= 48) && ((a) <= 57))
#define IF_SPLIT(a) \
(((a) == ' ') || ((a) == '\n'))
#define IF_DOT(a) \
((a) == '.')

#define TO_NUM(a) ((a)-48)

#define IP_BCAST 0xFFFFFFFF
#define IP_HIGH 0x80000000

#define IP_FMT "%hhu.%hhu.%hhu.%hhu"
#define LE_IP_FMT_STR(ip)                                                      \
  ((u8 *)&(ip))[3], ((u8 *)&(ip))[2], ((u8 *)&(ip))[1], ((u8 *)&(ip))[0]

#define BE_IP_FMT_STR(ip)                                                      \
  ((u8 *)&(ip))[0], ((u8 *)&(ip))[1], ((u8 *)&(ip))[2], ((u8 *)&(ip))[3]
#define NET_IP_FMT_STR(ip) BE_IP_FMT_STR(ip)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define HOST_IP_FMT_STR(ip) LE_IP_FMT_STR(ip)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define HOST_IP_FMT_STR(ip) BE_IP_FMT_STR(ip)
#endif

#endif //PRJ_10_IP_H
