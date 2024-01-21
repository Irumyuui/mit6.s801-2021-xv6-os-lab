#include "kernel/types.h"
#include "user/user.h"

int main() {
    const char* msg = "a";
    char buf[2] = {};

    int pipe_p_to_c[2], pipe_c_to_p[2];
    
    // 0:= read fd
    // 1:= write fd
    pipe(pipe_p_to_c);
    pipe(pipe_c_to_p);

    // == 0 := ch p
    if (fork() == 0) {
        read(pipe_p_to_c[0], buf, 1);
        printf("%d: received ping\n", getpid());   
        write(pipe_c_to_p[1], msg, 1);
        close(pipe_p_to_c[1]);
        close(pipe_c_to_p[0]);
    } else {
        write(pipe_p_to_c[1], msg, 1);
        read(pipe_c_to_p[0], buf, 1);
        printf("%d: received pong\n", getpid());
        close(pipe_c_to_p[1]);
        close(pipe_p_to_c[0]);
    }

    exit(0);
}
