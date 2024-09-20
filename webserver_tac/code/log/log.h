#ifndef LOG_H
#define LOG_H

#include "blockqueue.h"
#include <assert.h>
#include <mutex>
#include <stdarg.h> // vastart va_end
#include <string.h>
#include <string>
#include <sys/stat.h> //mkdir
#include <sys/time.h>
#include <thread>

#include "../buffer/buffer.h"

class Log
{
  public:
    void init(int level, const char *path = "./log", const char *suffix = ".log", int maxQueueCapacity = 1024);

    static Log *Instance();
    static void FlushLogThread(); // 异步写线程的调用函数

    void write(int level, const char *format, ...);
    void flush();   // 刷新log文件，如果是异步，还得唤醒消费者。

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen()
    {
        return isOpen_;
    }

  private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

  private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char *path_;   // 路径名
    const char *suffix_; // 扩展名

    int MAX_LINES_;

    unsigned int lineCount_;
    int toDay_;

    bool isOpen_;

    Buffer buff_; // 缓冲区
    int level_;     //最大日志level
    bool isAsync_;  // 是否异步

    FILE *fp_;
    std::unique_ptr<BlockDeque<std::string>> deque_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;
};

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口
// ...表示可变参数，__VA_ARGS__就是将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。

#define LOG_BASE(level, format, ...)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        Log *log = Log::Instance();                                                                                    \
        if (log->IsOpen() && log->GetLevel() <= level)                                                                 \
        {                                                                                                              \
            log->write(level, format, ##__VA_ARGS__);                                                                  \
            log->flush();                                                                                              \
        }                                                                                                              \
    } while (0);

#define LOG_DEBUG(format, ...)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        LOG_BASE(0, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_INFO(format, ...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        LOG_BASE(1, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_WARN(format, ...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        LOG_BASE(2, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_ERROR(format, ...)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        LOG_BASE(3, format, ##__VA_ARGS__)                                                                             \
    } while (0);

#endif // LOG_H