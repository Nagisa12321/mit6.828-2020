#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

char *
name(char *path)
{
    char            *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    return p;
}

int 
find(char *path, char *filename) {
    struct dirent       de;
    struct stat         st;
    int                 fd;
    char                buf[256], *p;
    char                *n;
    if ((fd = open(path, 0)) < 0) {
        printf("find: cannot open %s\n", path);
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return -1;
    }

    if (st.type == T_FILE) {
        if (!strcmp(filename, name(path))) {
            printf("%s\n", path);
        }
    } else if (st.type == T_DIR) {
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            return -1;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            n = name(buf);
            if (!strcmp(n, ".") || !strcmp(n, ".."))
                continue;
            find(buf, filename);
        }
    }

    close(fd);
    return 0;
}

int 
main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("please type find <path> <filename>.\n");
        exit(1);
    }

    if (find(argv[1], argv[2]) == -1) {
        printf("find error!\n");
        exit(1);
    }

    // printf("i am here\n");
    exit(0);
}