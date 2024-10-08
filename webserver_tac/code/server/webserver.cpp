#include "webserver.h"

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int sqlPort, const char *sqlUser,
                     const char *sqlPwd, const char *dbName, int connPoolNum, int threadNum, bool openLog, int logLevel,
                     int logQueSize)
    : port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false), timer_(new HeapTimer()),
      threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{

    srcDir_ = new char[256];
    strcpy(srcDir_, "/home/kuda/cplusplus/webserver_tac/resources/");

    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;

    if (openLog)
    {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        LOG_INFO("========== Server init ==========");
        LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger ? "true" : "false");
        LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", (listenEvent_ & EPOLLET ? "ET" : "LT"),
                 (connEvent_ & EPOLLET ? "ET" : "LT"));
        LOG_INFO("LogSys level: %d", logLevel);
        LOG_INFO("srcDir: %s", HttpConn::srcDir);
        LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
    }

    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    InitEventMode_(trigMode);
    if (!InitSocket_())
    {
        isClose_ = true;
        LOG_ERROR("========== Server init error!==========");
    }
}

WebServer::~WebServer()
{
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode)
{
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start()
{
    int timeMS = -1; /* epoll wait timeout == -1 无事件将阻塞 */
    if (!isClose_)
    {
        LOG_INFO("========== Server start ==========");
    }
    while (!isClose_)
    {
        if (timeoutMS_ > 0)
        {
            timeMS = timer_->GetNextTick(); // 获取下一个事件剩余时间
        }
        int eventCnt = epoller_->Wait(timeMS);
        for (int i = 0; i < eventCnt; i++)
        {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listenFd_)
            {
                DealListen_();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if (events & EPOLLIN)
            {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if (events & EPOLLOUT)
            {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else
            {
                std::cout << "Unexpected event" << std::endl;
            }
        }
    }
}

void WebServer::SendError_(int fd, const char *info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    close(fd);
}

void WebServer::CloseConn_(HttpConn *client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    users_[fd].init(fd, addr);
    char ip[24] = {0};
    LOG_INFO("Connect from %s", inet_ntop(AF_INET, &addr.sin_addr.s_addr, ip, sizeof(ip)));
    if (timeoutMS_ > 0)
    {
        // 将新连接添加到定时器中
        timer_->add(fd, timeoutMS_, [this, capture0 = &users_[fd]] { CloseConn_(capture0); });
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
}

void WebServer::DealListen_()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if (fd <= 0)
        {
            return;
        }
        else if (HttpConn::userCount >= MAX_FD)
        {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void WebServer::ExtentTime_(HttpConn *client)
{
    assert(client);
    // 当连接有新的事件时更新定时器
    if (timeoutMS_ > 0)
    {
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

void WebServer::DealRead_(HttpConn *client)
{
    assert(client);

    // 读取之后再放进去
    // int ret = -1;
    // int readErrno = 0;
    // ret = client->read(&readErrno);
    // if (ret <= 0 && readErrno != EAGAIN)
    // {
    //     CloseConn_(client);
    //     return;
    // }
    ///////////////////////////////////
    ExtentTime_(client);
    threadpool_->AddTask([this, client] { OnRead_(client); });
}

void WebServer::DealWrite_(HttpConn *client)
{
    assert(client);

    // 直接写
    // int ret = -1;
    // int writeErrno = 0;
    // ret = client->write(&writeErrno);
    // if (ret < 0)
    // {
    //     if (writeErrno == EAGAIN)
    //     {
    //         /* 写缓冲区满了 继续传输 */
    //         epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    //     }
    //     else
    //     {
    //         CloseConn_(client);
    //     }
    //     return;
    // }
    /////////////////////////////
    ExtentTime_(client);
    threadpool_->AddTask([this, client] { OnWrite_(client); });
}

void WebServer::OnRead_(HttpConn *client)
{
    assert(client);

    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN)
    {
        CloseConn_(client);
        return;
    }

    OnProcess(client);
}

void WebServer::OnProcess(HttpConn *client)
{
    if (client->process())
    {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    }
    else
    {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn *client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0)
    {
        /* 传输完成 */
        if (client->IsKeepAlive())
        {
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0)
    {
        if (writeErrno == EAGAIN)
        { // 写缓冲区满了
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_()
{
    int ret;
    struct sockaddr_in addr;
    if (port_ > 65535 || port_ < 1024)
    {
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = {0};
    if (openLinger_)
    {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0)
    {
        close(listenFd_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    if (ret == -1)
    {
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0)
    {
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0)
    {
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    return true;
}

int WebServer::SetFdNonblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}