#include <stdio.h>
#include "ThreadPool.h"

int work(void *args)
{
    int id = (int)args;
    printf("now in work %d\n", id);
    return 0;
}

int main()
{
    struct ThreadPool *pool = ThreadPoolInit(4);
    
    for (int i = 0; i < 40; i++)
    {
        int id = i;
        ThreadPoolPush(pool, work, (void *)id, sizeof(id));
    }
    printf("add work added\n");

    int ret = Run(pool);
    Wait(pool);
    
    printf("all inwork\n");
    return 0;
}
