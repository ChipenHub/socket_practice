#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    // 1. open shm
    int shmid = shmget(100, 0, 0);
    // 2. relate shm with cur process
    void* ptr = shmat(shmid, NULL, 0);
    // 3. write on shm
    printf("content: %s\n", (char *)ptr);
    
    
    printf("press any key to continue ...\n");
    getchar();
    // 4. release relevance

    shmdt(ptr);
    // 5. destory shm
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}