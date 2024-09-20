#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "ThreadPool.hpp"

void work(int cfd, sockaddr_in client_addr)
{
    // 打印客户端的地址信息
    char ip[24] = {0};
    printf("客户端的IP地址: %s, 端口: %d\n",
           inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof(ip)),
           ntohs(client_addr.sin_port));

    // 5. 和客户端通信
    while (1)
    {
        // 接收数据
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        int len = read(cfd, buf, sizeof(buf));
        if (len > 0)
        {
            printf("客户端say: %s\n", buf);
            write(cfd, buf, len);
        }
        else if (len == 0)
        {
            printf("客户端断开了连接...\n");
            break;
        }
        else
        {
            perror("read");
            break;
        }
    }

    close(cfd);
}

int main()
{
    // 1. 创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        exit(0);
    }

    // 2. 将socket()返回值和本地的IP端口绑定到一起
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(10000);      // 大端端口
    addr.sin_addr.s_addr = INADDR_ANY; // 这个宏的值为0 == 0.0.0.0
                                       //    inet_pton(AF_INET, "192.168.237.131", &addr.sin_addr.s_addr);
    int ret = bind(lfd, (sockaddr *)&addr, sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        exit(0);
    }

    // 3. 设置监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        exit(0);
    }

    ThreadPool pool(4);

    // 4. 阻塞等待并接受客户端连接
    sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    while (1)
    {
        int cfd = accept(lfd, (sockaddr *)&client_addr, &client_size);
        if (cfd == -1)
        {
            perror("accept");
            close(lfd);
            break;
        }
        pool.submit(work,cfd,client_addr);
    }

    return 0;
}