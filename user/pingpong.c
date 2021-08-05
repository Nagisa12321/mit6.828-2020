#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int     pid, n;
    int     fd1[2];
    int     fd2[2];
    char    buf[1024] = { 0 };

    pipe(fd1);
    pipe(fd2);
    if ((pid = fork()) < 0) {
        printf("fork error!\n");
        exit(1);
    } else if (pid > 0) {   /* parent */
        close(fd1[1]);
        close(fd2[0]);
        write(fd2[1], "a", 1);
        // read from fd1[0]
        if ((n = read(fd1[0], buf, 2)) == -1) {
            printf("parent read error!\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        exit(0);
    } else {                /* children */
        close(fd2[1]);
        close(fd1[0]);
        // read from fd2[0]
        if ((n = read(fd2[0], buf, 2)) == -1) {
            printf("child read error! \n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        // write to fd1[1]
        write(fd1[1], buf, n);
        exit(0);
    }
}