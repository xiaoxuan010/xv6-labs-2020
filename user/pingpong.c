#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int pipe_downwards[2], pipe_upwards[2];

    pipe(pipe_downwards);
    pipe(pipe_upwards);

    if (fork() == 0)
    {
        close(pipe_downwards[1]);
        close(pipe_upwards[0]);
        char buf;
        if (read(pipe_downwards[0], &buf, 1) != 1)
        {
            printf("%d: read error\n", getpid());
            exit(1);
        }
        else
        {
            printf("%d: received ping\n", getpid());
            if (write(pipe_upwards[1], &buf, 1) != 1)
            {
                printf("%d: write error\n", getpid());
                exit(1);
            }
        }
    }
    else
    {
        close(pipe_downwards[0]);
        close(pipe_upwards[1]);
        if (write(pipe_downwards[1], "", 1) != 1)
        {
            printf("%d: write error\n", getpid());
            exit(1);
        }
        char buf;
        if (read(pipe_upwards[0], &buf, 1) != 1)
        {
            printf("%d: read error\n", getpid());
        }
        else
        {
            printf("%d: received pong\n", getpid());
        }
        wait(0);
    }
    exit(0);
}