#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h> // vastart va_end
#include <assert.h>
#include <sys/stat.h> //mkdir
#include "blockqueue.hpp"
class log
{
public:
    //单例模式
    static log &Instance()
    {
        static log log_instance;
        return log_instance;
    }
    static void *flush_log_thread()
    {
        log::Instance().async_write_log();
    }
    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    //写入
    void write_log(int level, const char *format, ...);
    //强迫写入缓冲区数据
    void flush(void);
    int GetLevel();
    void SetLevel(int level);

private:
    log();
    ~log();
    void *async_write_log();
    static const int MAX_PATH = 128;    //最大路径
    static const int MAX_LOGNAME = 128; //最大路径

    std::unique_ptr<BlockDeque<std::string>> m_log_queue;
    std::unique_ptr<std::thread> m_write_thread;
    std::mutex m_mutex;
    FILE *m_fp; //打开的log文件指针

    char m_dir_name[MAX_PATH];    //路径名
    char m_log_name[MAX_LOGNAME]; // log文件名

    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天
    bool m_is_async;    //是否同步标志位
    bool m_close_flag;  //关闭日志 ---?
    int m_level;
    bool m_is_open;
    char* m_buf;//缓冲区



}

#define LOG_BASE(level, format, ...) \
    do {\
        log& log_instance = log::Instance();\
        if (log_instance.GetLevel() <= level) {\
            log_instance.write_log(level, format, ##__VA_ARGS__); \
            log_instance.flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

;
#endif