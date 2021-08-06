#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXLINE 512

int main(int argc, char *argv[]) {
    char        *cmd;
    char        *cmd_argv[MAXARG];
    char        *argvbuf;
    int         cmdidx;
    int         cmd_default;
    char        ch;
    int         bufidx;
    // int         i;

    cmdidx = 0;
    cmd_default = 1;
    // read the cmd
    if (argc == 1) {
        printf("no prarms, so the cmd is default <echo> \n");
        cmd = "echo";
    } else {
        cmd = argv[1];
        cmd_argv[cmdidx++] = cmd;
    } 
    if (argc > 2) {
        cmd_default++;
        cmd_argv[cmdidx++] = argv[2];
    }

    // read on the stdin, 
    // and run the cmd
    // stdin = 0
    while (1) {
        bufidx = 0;
        argvbuf = (char *) malloc(sizeof(char) * MAXARG);
        while (1) {
            if (read(0, &ch, 1) == -1) {
                printf("read error!\n");
                exit(0);
            }
            // printf("read a ch: [%c], [%d]\n", ch, ch);
            if (ch == 0) 
                exit(0);
            if (ch == ' ' || ch == '\n') {
                if (ch == '\n' && !bufidx) 
                    exit(0);
                argvbuf[bufidx] = 0;
                cmd_argv[cmdidx++] = argvbuf;
                if (fork() == 0) {
                    // printf("will run [%s ...]\n", cmd);
                    if (exec(cmd, cmd_argv) == -1) {
                        printf("exec error\n");
                        exit(1);
                    }
                }
                wait(0);
                argvbuf = (char *) malloc(sizeof(char) * MAXARG);
                memset(argvbuf, 0, sizeof(char) * MAXARG);
                bufidx = 0;
                cmdidx = cmd_default;
            } else {
                argvbuf[bufidx++] = ch;
            }
        }
    }
    exit(0);
}