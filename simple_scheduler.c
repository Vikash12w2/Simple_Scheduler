#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h> 
#include <sys/wait.h>


typedef struct{
    int pid;
    int ExecutionTime;
    int WaitTime;
}Process;

#define MAX_QUEUE_SIZE 1000

typedef struct {
    Process comands[MAX_QUEUE_SIZE];
    int front, rear;
} ReadyQueue;

//to initialize a queue
void initializeQueue(ReadyQueue *q)
{
    q->front = -1;
    q->rear = -1;
}

bool isEmpty(ReadyQueue *q){
    bool a = q->front == -1;
    return a;
}

bool isFull(ReadyQueue *q){
    bool a = (q->rear + 1) % MAX_QUEUE_SIZE == q->front;
    return a;
}

//to enqueue an element
void enqueue(ReadyQueue *q,Process com){
    if (isFull(q)){
        printf("Queue is full.\n");
        return;
    }
    if (isEmpty(q)){
        q->front = 0;
        q->rear = 0;
    }
    else{
        q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    }
    q->comands[q->rear] = com;
}

Process dequeue(ReadyQueue *q)
{
    if (isEmpty(q))
    {
        printf("Ready Queue is empty\n");
        exit(0);
    }
    Process com = q->comands[q->front];
    if (q->front == q->rear){
        q->front = -1;
        q->rear = -1;
    }
    else{
        q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    }
    return com;
}

bool isPresent(ReadyQueue *q, int pid) {
    if (isEmpty(q)) {
        return false;
    }

    int a = q->front;

    while (a <= q->rear) {
        if (q->comands[a].pid == pid) {
            return true; // found
        }
        a++;
    }

    return false; // not found
}

int main(int argc, char *argv[]){

    int pid_scheduler = getpid();
    int fd1 = open("system_parameter", O_WRONLY);
    write(fd1, &pid_scheduler, sizeof(int));
    close(fd1);



    ReadyQueue q;

    initializeQueue(&q);

    int fd3 = open("system_parameter", O_RDONLY);
    int NCPU, TSLICE;
    read(fd3, &NCPU, sizeof(int));
    read(fd3, &TSLICE, sizeof(int));
    close(fd3);

    int fd[2];
    if (pipe(fd)==-1){
        printf("Pipe Failed\n");
        exit(0);
    }

    while(1){
        int pid_process;
        int fd2 = open("pid_processes", O_RDONLY);
        
        if (fd2 == -1){
            printf("Error while oprning file descriptor ---> scheduler\n");
            exit(0);
        }

        if (read(fd2, &pid_process, sizeof(int)) == -1)
        {
            printf("Error while reading pocesse pids ---> scheduler\n");
            exit(0);
        }

        Process pro1;
        pro1.pid = pid_process;
        pro1.ExecutionTime = 0;
        pro1.WaitTime = 0;
        enqueue(&q, pro1);

        close(fd2);
        close(fd[0]);
        write(fd[1],&pro1,sizeof(Process));
        close(fd[1]);
        kill(pid_scheduler, SIGSTOP);
    }

    int pid = fork();
    if (pid == -1){
        perror("Could not fork\n");
    }

    if(pid==0){
        ReadyQueue RunQueue;
        ReadyQueue CompletedQueue;

        while(!isEmpty(&q)){
            Process pro2;
            read(fd[0],&pro2,sizeof(Process));
            if(!isPresent(&q,pro2.pid)){
                enqueue(&q,pro2);
            }
            for(int i = 0; i <= NCPU; i++){
                Process pro3;
                pro3=dequeue(&q);
                kill(pro3.pid,SIGCONT);
                enqueue(&RunQueue,pro3);
            }

            usleep(TSLICE*1000);

            for(int i=NCPU+1;i<=q.rear;i++){
                Process pro4;
                pro4.WaitTime += TSLICE;
            }

            for(int i=0; i <= NCPU; i++){
                Process pro5;
                pro5=dequeue(&RunQueue);
                kill(pro5.pid,SIGSTOP);
                enqueue(&RunQueue,pro5);
            }

            //Update process info
            for(int j = 0; j <= NCPU; j++){
                int status;
                Process process;
                process=dequeue(&RunQueue);
                int result = waitpid(process.pid, &status, 0);

                if(WIFEXITED(status)){
                    process.ExecutionTime += TSLICE;
                    enqueue(&CompletedQueue,process);
                }
                else{
                    process.ExecutionTime += TSLICE;
                    enqueue(&q,process);
                }
            }
        }

        int fd = open("final_information",O_WRONLY);
        if(fd == -1){
            printf("Error while opening file descriptor\n");
            exit(0);
        }

        int b = CompletedQueue.rear + 1;
        if(write(fd, &b, sizeof(int)) == -1){
            printf("Error while writing in the file\n");
            exit(0);
        }

        if(write(fd, &CompletedQueue, sizeof(ReadyQueue)) == -1){
            printf("Error while writing final_information\n");
            exit(0);
        }

        close(fd);
    }
    else{
        wait(NULL);
    }
}