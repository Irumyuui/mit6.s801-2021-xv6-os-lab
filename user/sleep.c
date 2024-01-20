#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: sleep digit");
        exit(1);
    }

    int is_digit = 1, sec = 0;
    for (const char* s = argv[1]; *s; s ++) {
        if (*s >= '0' && *s <= '9') {
            sec = sec * 10 + *s - '0';
        } else {
            is_digit = 0;
            break;
        }
    }

    if (!is_digit) {
        printf("argv must be int");
        exit(1);
    }

    sleep(sec);

    exit(0);
}
