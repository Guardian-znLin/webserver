#ifndef LOCKER_H
#define LOCKER_H

#include <unistd.h>
#include <pthread.h>
#include <exception>
#include <semaphore.h>

//线程同步机制封装类

//互斥锁类
class locker{
private:
    pthread_mutex_t m_mutex;

public:
    locker();
    ~locker();

    bool lock();
    bool unlock();
    pthread_mutex_t& get();
};

//条件变量类
class condition{
private:
    pthread_cond_t m_cond;
public:
    condition(){
        if(pthread_cond_init(&m_cond,nullptr) != 0){
            throw std::exception();
        }
    }
    ~condition(){
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t& mutex){
        return pthread_cond_wait(&m_cond, &mutex) == 0;
    }
    bool timewait(pthread_mutex_t& mutex,const timespec t){
        return pthread_cond_timedwait(&m_cond,&mutex, &t) == 0;
    }

    bool signal(pthread_mutex_t& mutex){
        return pthread_cond_signal(&m_cond) == 0;
    }
    
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};

//信号量类
class sem{
private:
    sem_t m_sem;
public:
    sem(){
        if(sem_init(&m_sem,0,0)!=0){
            throw std::exception();
        }
    }
    sem(int value){
        if(sem_init(&m_sem,0,value)!=0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }

    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    bool post(){
        return sem_post(&m_sem) == 0;
    }
};
#endif