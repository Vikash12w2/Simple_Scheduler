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

void handle_new_process(ReadyQueue *q) {
    int fd = open("pid_processes", O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            // File doesn't exist yet, that's okay
            return;
        }
        perror("Error opening process_pids file");
        return;
    }

    int pid_process;
    ssize_t read_result;

    // Read all PIDs from the file
    while ((read_result = read(fd, &pid_process, sizeof(int))) > 0) {
        Process new_process;
        new_process.pid = pid_process;
        new_process.ExecutionTime = 0;
        new_process.WaitTime = 0;
        
        if (!isPresent(q, pid_process)) {
            enqueue(q, new_process);
            printf("Added process PID %d to queue\n", pid_process);
        }
    }

    close(fd);

    // Clear the file after reading
    int fd_clear = open("pid_processes", O_WRONLY | O_TRUNC);
    if (fd_clear != -1) {
        close(fd_clear);
    }
}

int main(int argc, char *argv[]){
    int fd1 = open("system_parameter", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd1 == -1) {
        perror("Error creating system_parameter file");
        exit(1);
    }

    int pid_scheduler = getpid();
    if (write(fd1, &pid_scheduler, sizeof(int)) == -1) {
        perror("Error writing scheduler PID");
        close(fd1);
        exit(1);
    }
    close(fd1);

    printf("Scheduler started with PID: %d\n", pid_scheduler);
    
    // Create the process_pids file
    int fd_pids = open("pid_processes", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_pids == -1) {
        perror("Error creating process_pids file");
        exit(1);
    }
    close(fd_pids);



    ReadyQueue ready_queue;
    ReadyQueue completed_queue;
    initializeQueue(&ready_queue);
    initializeQueue(&completed_queue);

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
        int fd2 = open("pid_processes", O_RDONLY);
        
        if (fd2 != -1) {
            pid_t new_pid;
            while (read(fd2, &new_pid, sizeof(pid_t)) > 0) {
                Process new_proc = {
                    .pid = new_pid,
                    .ExecutionTime = 0,
                    .WaitTime = 0
                };
                if (!isPresent(&ready_queue, new_pid)) {
                    enqueue(&ready_queue, new_proc);
                    printf("Added process %d to ready queue\n", new_pid);
                }
            }
        close(fd2);

        truncate("process_pids", 0);
        }

     // Schedule processes
        int running_count = 0;
        while (!isEmpty(&ready_queue) && running_count < NCPU) {
            Process proc = dequeue(&ready_queue);
            
            // Check if process still exists
            if (kill(proc.pid, 0) == 0) {
                kill(proc.pid, SIGCONT);
                proc.ExecutionTime += TSLICE;
                running_count++;
                
                // Wait for time slice
                usleep(TSLICE * 1000);
                
                // Stop process and check status
                kill(proc.pid, SIGSTOP);
                
                int status;
                pid_t result = waitpid(proc.pid, &status, WNOHANG);
                
                if (result == 0) {
                    // Process still running
                    enqueue(&ready_queue, proc);
                } else if (result > 0 && WIFEXITED(status)) {
                    // Process completed
                    enqueue(&completed_queue, proc);
                }
            }
        }
        
        // Update wait times for processes in ready queue
        if (!isEmpty(&ready_queue)) {
            for (int i = ready_queue.front; i <= ready_queue.rear; i++) {
                ready_queue.comands[i].WaitTime += TSLICE;
            }
        }
        
        // Write completed process information
        int fd_info = open("final_information", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_info != -1) {
            int size = completed_queue.rear - completed_queue.front + 1;
            write(fd_info, &size, sizeof(int));
            write(fd_info, &completed_queue, sizeof(ReadyQueue));
            close(fd_info);
        }
        
        // Small sleep to prevent CPU hogging
        usleep(1000);
    }   
    
    return 0;
}
