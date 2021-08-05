#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char *argv[]) {
    int sec;
    if (argc <= 1) {
        fprintf(2, "prarms is less than 1. \n");
        exit(1);
    }
    sec = atoi(argv[1]);
    fprintf(2, "will sleep %d sec \n", sec);
    sleep(sec);
    fprintf(2, "sleep successfully \n");
    exit(0);
}