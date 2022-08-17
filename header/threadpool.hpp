#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "locker.h"
#include <pthread.h>
#include <exception>
#include <list>
#include <cstdio>

template <typename T>
class threadpool
{
private:
    int m_thread_number;

    //线程池 数组
    pthread_t *m_threads;

    //请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list<T *> m_workqueue;

    //互斥锁
    locker m_queuelocker;

    //信号量，是否有任务要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;

    static void *worker(void *arg);
    //从请求队列中出队，并process
    void run();

public:
    threadpool(int threadnum = 8, int max_requests = 10000);
    ~threadpool();
    //追加到请求队列
    bool append(T *request);
};

template <typename T>
threadpool<T>::threadpool(int threadnum, int max_requests) : m_thread_number(threadnum), m_max_requests(max_requests), m_stop(false), m_threads(nullptr)
{
    if ((threadnum <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }
    //创建threadnum个线程，设置为线程脱离detach
    for (int i = 0; i < threadnum; ++i)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(&m_threads[i], nullptr, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *ptr = (threadpool *)arg;
    ptr->run();
    return ptr;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (request == nullptr)
        {
            continue;
        }
        request->process();
    }
}

#endif