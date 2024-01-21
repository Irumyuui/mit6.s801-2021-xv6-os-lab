#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include <float.h>

char* get_filename(char* path) {
    static char buf[DIRSIZ + 1] = {};
    
    char* p;
    for (p = path + strlen(path); p >= path && *p != '/'; p --);
    p ++;

    if (strlen(p) >= DIRSIZ) {
        return p;
    }
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));

    return buf;
}

void find(char *path, char *tar) {
    char buf[512];
    int fd;
    struct stat st;
    
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_DEVICE:
        case T_FILE:
            if (strcmp(tar, get_filename(path)) == 0) {
                printf("%s\n", path);
            }
            break;

        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            
            char* p = buf + strlen(buf);
            *p ++ = '/';
            
            struct dirent de;
            while(read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }

                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }

                char* name = de.name;
                
                if (st.type != T_DIR) {
                    // goto TAR_FILE;
                    if (strcmp(tar, name) == 0) {
                        printf("%s\n", buf);
                    }
                    continue;
                }
                
                if (strcmp(name, ".") && strcmp(name, "..")) {
                    find(buf, tar);
                }
            }

            break;
        default:
            break;
    }

    close(fd);
    return;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "useage: find path/to/dir file_name\n");
    } else {
        find(argv[1], argv[2]);
    }
    exit(0);
}
