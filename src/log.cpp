#include "../header/log.h"
using std::lock_guard;
using std::mutex;

log::log()
{
    m_count = 0;
    m_is_async = false;
    m_split_lines = 0;    //日志最大行数
    m_log_buf_size = 0;   //日志缓冲区大小
    m_today = 0;          //因为按天分类,记录当前时间是那一天
    m_close_flag = false; //关闭日志
    int m_level = 0;
}
log::~log()
{
    assert(m_fp == nullptr);
    while (!m_log_queue->empty())
    {
        m_log_queue->flush();
    };
    m_log_queue->Close();
    lock_guard<mutex> locker(m_mutex);
    flush();
    fclose(m_fp);
}

void *log::async_write_log()
{
    std::string single_log;
    //从阻塞队列中取出一个日志string，写入文件
    while (m_log_queue->pop(single_log))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        fputs(single_log.c_str(), m_fp);
    }
}

int log::GetLevel()
{
    lock_guard<mutex> locker(m_mutex);
    return m_level;
}

void log::SetLevel(int level)
{
    lock_guard<mutex> locker(m_mutex);
    m_level = level;
}

bool log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        if (!m_log_queue)
        {
            m_log_queue = move(std::unique_ptr<BlockDeque<std::string>>(new BlockDeque<std::string>));
            // flush_log_thread为回调函数,这里表示创建线程异步写日志
            m_write_thread = move(std::unique_ptr<std::thread>(new std::thread(log::flush_log_thread)));
        }
    }
    m_close_flag = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm my_tm = *localtime(&t);

    const char *p = strrchr(file_name, '/');
    char log_full_name[MAX_PATH + MAX_LOGNAME] = {0};
    if (p == NULL) //保存当前目录下
    {
        snprintf(log_full_name, MAX_PATH + MAX_LOGNAME - 1, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(m_log_name, p + 1);
        strncpy(m_dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, MAX_PATH + MAX_LOGNAME - 1, "%s%d_%02d_%02d_%s", m_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_log_name);
    }
    m_today = my_tm.tm_mday;

    flush();
    m_fp = fopen(log_full_name, "a"); // O_WRONLY | O_CREAT | O_APPEND
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm my_tm = *localtime(&t);
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    
    std::unique_lock<mutex> unique_lock(m_mutex);

    m_count++;

    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) // everyday log
    {
        
        char new_log[MAX_PATH + MAX_LOGNAME] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, MAX_PATH + MAX_LOGNAME-1, "%s%s%s", m_dir_name, tail, m_log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, MAX_PATH + MAX_LOGNAME-1, "%s%s%s.%lld", m_dir_name, tail, m_log_name, m_count / m_split_lines);
        }
        flush();
        fclose(m_fp);
        m_fp = fopen(new_log, "a");
        assert(m_fp != nullptr);
    }
    unique_lock.unlock();
    

    va_list valst;
    va_start(valst, format);

    std::string log_str;
    unique_lock.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    unique_lock.unlock();

    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push_back(log_str);
    }
    else
    {
        unique_lock.lock();
        fputs(log_str.c_str(), m_fp);
        unique_lock.unlock();
    }

    va_end(valst);
}

void log::flush(void)
{
    lock_guard<mutex> lock(m_mutex);
    //强制刷新写入流缓冲区
    fflush(m_fp);
}