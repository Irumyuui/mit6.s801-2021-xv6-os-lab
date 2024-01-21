#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    char* xargv[MAXARG] = {};
    for (int i = 1; i < argc; i ++)
        xargv[i - 1] = argv[i];

    char buf[512] = {};
    while (gets(buf, 512)) {
        int len = strlen(buf);
        if (len == 0)
            break;
        if (buf[len - 1] == '\n')
            buf[len - 1] = '\0';
        
        xargv[argc - 1] = buf;
        if (fork() == 0) {
            exec(argv[1], xargv);
        } else {
            wait(0);
        }
    }

    exit(0);
}
