#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

/**
 * @brief 创建子进程并执行指定命令
 * @param cmdArgv 要执行的命令及其参数数组
 */
void forkAndExec(char *cmdArgv[])
{
    if (fork() == 0)
    {
        exec(cmdArgv[0], cmdArgv);
        exit(0);
    }
    else
    {
        wait(0);
    }
}

int prepareBaseCmdArgv(int baseC, char *baseArgv[], int cmdC, char *cmdArgv[])
{
    for (int i = 0; i < baseC && cmdC < MAXARG - 1; i++)
    {
        cmdArgv[cmdC++] = baseArgv[i];
    }
    return cmdC;
}

int prepareExtraCmdArgv(char *buf, int bufLen, int cmdC, char *cmdArgv[])
{
    char *p = buf;
    // char *end = buf + bufLen;
    while (*p && cmdC < MAXARG - 1)
    {
        // 跳过空格
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        // 记录参数起始位置
        cmdArgv[cmdC++] = p;
        // 找到参数结束位置
        while (*p && *p != ' ' && *p != '\t')
        {
            p++;
        }
        // 用空字符替换参数结束位置的空格或换行符
        if (*p)
        {
            *p = '\0';
            p++;
        }
    }
    return cmdC;
}

void prepareCmdArgv(char *buf, int bufLen, int baseC, char *baseArgv[], char *cmdArgv[])
{
    int cmdC = 0;
    cmdC = prepareBaseCmdArgv(baseC, baseArgv, cmdC, cmdArgv);
    cmdC = prepareExtraCmdArgv(buf, bufLen, cmdC, cmdArgv);
    cmdArgv[cmdC] = 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "usage: xargs command [args...]\n");
        exit(1);
    }

    // 准备基础命令参数
    char *base[MAXARG];
    int basec = 0;
    for (int i = 1; i < argc && basec < MAXARG - 1; i++)
    {
        base[basec++] = argv[i];
    }

    char buf[512];
    int n = 0;
    int cc;

    while ((cc = read(0, buf + n, 1)) > 0)
    {
        if (buf[n] == '\n')
        {
            buf[n] = '\0';
            char *cmd[MAXARG];
            prepareCmdArgv(buf, n, basec, base, cmd);
            forkAndExec(cmd);
            n = 0;
        }
        else
        {
            n += cc;
        }
    }
    exit(0);
}
