#include <stdio.h>

#include "trie.h"
#include "general.h"

// sample for unit test
char *test01 = "1.0.4.0 24 3\n";
char *test02 = "1.4.128.0 19 1\n";

// path of static dataset
//  knowing target program is in 'prj10/cmake-build-debug/'
char *dataset_path = "../forwarding-table.txt";

// static cache
u32 s_ip[NUM_REC], s_mask[NUM_REC], s_port[NUM_REC], a_port[NUM_REC];

int main(int argc, char **argv) {
    FILE *fptr = NULL;
    trie(fptr, dataset_path, s_ip, s_mask, s_port, a_port);

    return 0;
}
