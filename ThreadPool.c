#include "ThreadPool.h"

int BSemphereInit(struct BSemphere *bSem)
{
    pthread_mutex_init(&bSem->mutex, NULL);
    pthread_cond_init(&bSem->cond, NULL);
    bSem->val = 1;

    return OK;
}

int ThreadInit(struct ThreadPool *pool, struct MyThread *thread, int id)
{
    int ret;

    thread->pool = pool;
    thread->id = id;

    return OK;
}

int JobQueueInit(struct JobQueue *queue)
{
    int ret;

    queue->hasJob = (struct BSemphere*)malloc(sizeof(struct BSemphere));
    CHECK_RETURN((queue->hasJob == NULL), "malloc fail", FAIL);

    ret = BSemphereInit(queue->hasJob);
    CHECK_GOTO((ret != OK), "BSemphereInit fail", LAB_END);

    queue->jobCnt = 0;
    queue->front = NULL;
    queue->tail = NULL;

    pthread_mutex_init(&queue->rwMutex, NULL);

    return OK;

LAB_END:
    RELEASE(queue->hasJob);
    RELEASE(queue);
    return FAIL;
}

struct ThreadPool* ThreadPoolInit(int num)
{
    int ret;

    struct ThreadPool *pool = (struct ThreadPool *)malloc(sizeof(struct ThreadPool));
    CHECK_RETURN((pool == NULL), "malloc fail", NULL);

    pool->threads = (struct MyThread*)malloc(num * sizeof(struct MyThread));
    CHECK_GOTO((pool->threads == NULL), "malloc fail", LAB_END);

    pool->queue = (struct JobQueue *)malloc(sizeof(struct JobQueue));
    CHECK_GOTO((pool->queue == NULL), "malloc fail", LAB_END);

    pool->allIdle = (struct BSemphere *)malloc(sizeof(struct BSemphere));
    CHECK_GOTO((pool->allIdle == NULL), "malloc fail", LAB_END);

    ret = BSemphereInit(pool->allIdle);
    CHECK_GOTO((ret != OK), "BSemphere init fail", LAB_END);

    pool->thCnt = 0;
    for (int i = 0; i < num; i++)
    {
        ret = ThreadInit(pool, &pool->threads[i], i);
        CHECK_GOTO((ret != OK), "ThreadInit fail", LAB_END);

        pool->thCnt++;
    }

    ret = JobQueueInit(pool->queue);
    CHECK_GOTO((ret != OK), "JobQueueInit fail", LAB_END);

    pool->thCntAlive = 0;
    pool->thCntWork = 0;
    pool->keepAlive = 0;

    pthread_mutex_init(&pool->rwMutex, NULL);

    return pool;

LAB_END:
    for (int i = 0; i < pool->thCnt; i++)
    {
        RELEASE(&pool->threads[i]);
    }
    RELEASE(pool->queue);
    RELEASE(pool->allIdle);
    RELEASE(pool);
    return NULL;
}

void *ThreadDo(void *args)
{
    struct MyThread *this = (struct MyThread *)args;
    struct ThreadPool *pool = this->pool;
    
    pthread_mutex_lock(&pool->rwMutex);
    this->pool->thCntAlive++;
    pthread_mutex_unlock(&pool->rwMutex);

    printf("thread %d is online, now works: %d\n", this->id, pool->queue->jobCnt);

    while (pool->keepAlive)
    {
        SempthereWait(pool->queue->hasJob);

        if (pool->keepAlive)
        {
            struct Job* job = JobQueuePull(pool->queue);
            if (job != NULL)
            {
                pthread_mutex_lock(&pool->rwMutex);
                pool->thCntWork++;
                pthread_mutex_unlock(&pool->rwMutex);
            
                job->func(job->args);

                pthread_mutex_lock(&pool->rwMutex);
                pool->thCntWork--;
                if (pool->thCntWork == 0)
                {
                    if (pool->queue->jobCnt > 0)
                        SempthereSignal(pool->queue->hasJob);
                    else
                        SempthereSignal(pool->allIdle);
                }
                pthread_mutex_unlock(&pool->rwMutex);
            }
        }
    }

    printf("thread %d is offline\n", this->id);

    return NULL;
}

/* 在线程池中添加新任务, 注意args内存由用户申请, 此处深拷贝, 任务执行结束后释放 */
int ThreadPoolPush(struct ThreadPool *pool, int (*func)(void *), void *args, int size)
{
    struct Job *job = (struct Job*)malloc(sizeof(struct Job));
    CHECK_RETURN((job == NULL), "malloc fail", FAIL);

    job->func = func;
    job->args = args;
    job->next = NULL;

    return JobQueuePush(pool->queue, job);
}

int JobQueuePush(struct JobQueue *queue, struct Job *job)
{
    pthread_mutex_lock(&queue->rwMutex);
    if (queue->jobCnt == 0)
    {
        queue->front = job;
        queue->tail = job;
    }
    else
    {
        queue->tail->next = job;
        queue->tail = job;
    }
    queue->jobCnt++;
    SempthereSignal(queue->hasJob);
    pthread_mutex_unlock(&queue->rwMutex);

    return OK;
}

struct Job* JobQueuePull(struct JobQueue *queue)
{
    pthread_mutex_lock(&queue->rwMutex);

    if (queue->jobCnt == 0)
    {
        pthread_mutex_unlock(&queue->rwMutex);
        return NULL;
    }

    struct Job* job = queue->front;
    if (queue->jobCnt == 1)
    {
        queue->front = NULL;
        queue->tail = NULL;
    }
    else
    {
        queue->front = queue->front->next;
        job->next = NULL;
        SempthereSignal(queue->hasJob);
    }
    queue->jobCnt--;

    pthread_mutex_unlock(&queue->rwMutex);
    return job;
}

int SempthereWait(struct BSemphere *sem)
{
    pthread_mutex_lock(&sem->mutex);
    while (sem->val != 1)
    {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->val = 0;
    pthread_mutex_unlock(&sem->mutex);
}

int SempthereSignal(struct BSemphere *sem)
{
    pthread_mutex_lock(&sem->mutex);
    sem->val = 1;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}

int Run(struct ThreadPool *pool)
{
    pool->keepAlive = 1;
    for (int i = 0; i < pool->thCnt; i++)
    {
        pthread_create(&pool->threads[i].thread, NULL, ThreadDo, (void *)&pool->threads[i]);
        pthread_detach(pool->threads[i].thread);
    }

    while (pool->thCntAlive != pool->thCnt) {}
    if (pool->queue->jobCnt > 0)
    {
        SempthereSignal(pool->queue->hasJob);
    }
    return OK;
}

int Pause()
{

}

int Wait(struct ThreadPool *pool)
{
    while (pool->queue->jobCnt > 0 || pool->thCntWork > 0)
    {
        SempthereWait(pool->allIdle);
    }
}
