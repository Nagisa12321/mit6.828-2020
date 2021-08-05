#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main() {
    int         i;
    int         pfds[11][2];
    char        p = 0;
    char        n = 0;
    
    for (i = 0; i < 11; i++) {
        if (i == 0)
            pipe(pfds[i]);
        if (i != 10)
            pipe(pfds[i + 1]);
        if (fork() == 0) {  /* children */

            // read from the pfds[i][0]
            // and write to pfds[i + 1][1]
            // (if i != 10)
            if (read(pfds[i][0], &p, 1) == -1) {
                printf("read error\n");
                exit(1);
            }
            printf("prime %d\n", p);
            while (read(pfds[i][0], &n, 1)) {
                if (n == '&') {
                    write(pfds[i + 1][1], "&", 1);
                    exit(0);
                }
                // p does not divide n
                if ((n % p) && i != 10) {
                    write(pfds[i + 1][1], &n, 1);
                }
            }
        }

        if (i != 10)
            close(pfds[i + 1][1]);
        close(pfds[i][0]);
    }
    // printf("i am here\n");

    // write to the first child
    for (i = 2; i <= 35; i++) {
        write(pfds[0][1], &i, 1);
    }
    write(pfds[0][1], "&", 1);
    close(pfds[0][1]);

    // wait for 11 children
    for (i = 0; i < 11; i++) {
        wait(0);
    }
    exit(0);
}