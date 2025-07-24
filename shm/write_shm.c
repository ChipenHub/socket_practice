#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    // 1. create shm
    int shmid = shmget(100, 4096, IPC_CREAT|0664);
    // 2. relate shm with cur process
    void* ptr = shmat(shmid, NULL, 0);
    // 3. write on shm
    const char * tmp = "this is a demo for shm ...";
    memcpy(ptr, tmp, strlen(tmp) + 1);
    
    printf("press any key to continue ...\n");
    getchar();
    // 4. release relevance

    shmdt(ptr);
    // 5. destory shm
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
