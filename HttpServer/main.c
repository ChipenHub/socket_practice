#include "server.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("./server port\n");
        exit(0);
    }
    unsigned short port = atoi(argv[1]); // 转换成整形数
    epollRun(port);
    return 0;
}