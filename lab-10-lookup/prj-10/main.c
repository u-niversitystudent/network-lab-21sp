#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fread.h"
#include "trie.h"
#include "general.h"
#include "ip.h"

// sample for unit test
char *test01 = "1.0.4.0 24 3\n";
char *test02 = "1.4.128.0 19 1\n";

// path of static dataset
//  knowing target program is in 'prj10/cmake-build-debug/'
char *dataset_path = "../forwarding-table.txt";

// static cache
u32 s_ip[NUM_REC], s_mask[NUM_REC], s_port[NUM_REC], a_port[NUM_REC];

int main(int argc, char **argv) {
    int if_print_result = 0;
    for (int i = 0; i < argc && if_print_result==0 ; ++i) {
        if_print_result = strncmp(argv[i], "-r", 2);
    }

    FILE *fptr = NULL;

    printf("Exec Trie function...\n");
    trie(fptr, dataset_path, s_ip, s_mask, s_port, a_port);

    // decide if print result

    if (if_print_result) {
        // Result
        printf("--------\nResult:\n");
        for (int i = 0; i < NUM_REC; ++i) {
            if (s_port[i] == a_port[i]) printf("[same]");
            else printf("[diff]");

            printf("DATA: port=%d when ip="IP_FMT" mask=%d \n"
                   "     ROUTE: port=%d\n",
                   s_port[i], LE_IP_FMT_STR(s_ip[i]), s_mask[i],
                   a_port[i]);
        }
    }

    return 0;
}
