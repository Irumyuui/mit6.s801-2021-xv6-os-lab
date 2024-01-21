#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

__attribute__((noreturn))
void dfs(int rd) {
    int n;
    if (read(rd, &n, 4) == 0) {
        exit(0);
    }
    printf("prime %d\n", n);
    
    int p[2];
    pipe(p);
    
    if (fork() == 0) {
        close(p[0]);
        int m;
        while (read(rd, &m, 4)) {
            if (m % n) {
                write(p[1], &m, 4);
            }
        }
        close(rd);
        close(p[1]);
    } else {
        close(p[1]);
        dfs(p[0]);
    }

    wait(0);
    exit(0);
}

int main() {
    int p[2];
    pipe(p);

    for (int i = 2; i <= 35; i ++) {
        write(p[1], &i, 4);
    }
    close(p[1]);
    dfs(p[0]);

    wait(0);
    exit(0);
}
