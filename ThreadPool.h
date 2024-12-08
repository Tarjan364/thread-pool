#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define OK 0
#define FAIL 1

#define LOG(str) \
    do { \
        printf(str); \
        printf("\n"); \
    } while (0)

#define CHECK_RETURN(condition, str, ret) \
    do { \
        if (condition) \
        { \
            LOG(str); \
            return ret; \
        } \
    } while (0)

#define CHECK_GOTO(condition, str, LAB) \
    do { \
        if (condition) \
        { \
            LOG(str); \
            goto LAB; \
        } \
    } while (0)

#define RELEASE(pointer) \
    if (pointer != NULL) \
    { \
        free(pointer); \
    }

/* 数据结构定义 */

struct BSemphere {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int val;
};

struct Job {
    int (*func)(void *);
    void *args;
    struct Job* next;
};

struct JobQueue {
    int jobCnt;
    struct Job* front;
    struct Job* tail;
    pthread_mutex_t rwMutex;
    struct BSemphere *hasJob;
};

struct MyThread {
    struct ThreadPool *pool;
    pthread_t thread;
    int id;
};

struct ThreadPool {
    int thCnt;
    int thCntWork;
    int thCntAlive;
    int keepAlive;
    struct MyThread *threads;
    struct JobQueue *queue;
    pthread_mutex_t rwMutex;
    pthread_cond_t cond;
};

/* 函数定义 */

struct ThreadPool* ThreadPoolInit(int num);
int ThreadPoolRelease(struct ThreadPool *pool);
int ThreadPoolPush(struct ThreadPool *pool, int (*func)(void *), void *args, int size);

int ThreadInit(struct ThreadPool *pool, struct MyThread *thread, int id);

int JobQueueInit(struct JobQueue *queue);
int JobQueuePush(struct JobQueue *queue, struct Job *job);
struct Job* JobQueuePull(struct JobQueue *queue);

int BSemphereInit(struct BSemphere *bSem);
int SempthereWait(struct BSemphere *sem);
int SempthereSignal(struct BSemphere *sem);

int Run(struct ThreadPool *pool);

int Pause();

int Wait(struct ThreadPool *pool);