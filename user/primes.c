#include "kernel/types.h"
#include "user/user.h"

#define MINN 2
#define MAXN 280

void createThread(int p_reader);

int main(int argc, char *argv[])
{
    // 关闭不需要的标准输入和标准错误
    close(0);
    close(2);

    int down_pipe[2];
    pipe(down_pipe);

    if (fork() != 0)
    {
        // 父进程
        close(down_pipe[0]);
        for (int p = MINN; p <= MAXN; p++)
        {
            write(down_pipe[1], &p, sizeof(int));
        }
        close(down_pipe[1]);
        wait(0);
    }
    else
    {
        // 子进程
        close(down_pipe[1]);
        createThread(down_pipe[0]);
    }
    exit(0);
}

void createThread(int p_reader)
{
    int prime;
    int n = read(p_reader, &prime, sizeof(int));
    if (n <= 0)
    {
        close(p_reader);
        exit(0);
    }

    printf("prime %d\n", prime);

    int p;
    if (!read(p_reader, &p, sizeof(int)))
    {
        close(p_reader);
        exit(0);
    }
    int down_pipe[2];
    pipe(down_pipe);

    if (fork() == 0)
    {
        // 子进程
        close(down_pipe[1]);
        // 关闭继承自父进程的左侧读取器文件描述符；因为此子进程从down_pipe[0]读取
        close(p_reader);
        createThread(down_pipe[0]);
    }
    else
    {
        // 父进程
        close(down_pipe[0]);
        do
        {
            if (p % prime != 0)
            {
                write(down_pipe[1], &p, sizeof(int));
            }
        } while (read(p_reader, &p, sizeof(int)) != 0);
        close(down_pipe[1]);
        close(p_reader);
        wait(0);
    }
}