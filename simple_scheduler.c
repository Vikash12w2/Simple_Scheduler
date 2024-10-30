#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>


// Process struct represents a process in the ready queue
typedef struct Process {
    pid_t pid;
    int priority;
    struct Process* next;
} Process;

// Global variables
Process *ReadyQueue = NULL, *LastProcess = NULL;
int NCPU, TSLICE, RunCount = 0;
pid_t RunningProcesses[1000];

//get PID of a process
pid_t get_process_pid(Process *process) {
    return process->pid;
}

int get_process_priority(Process *process) {
    return process->priority;
}

// submit process to scheduler
void Submit(pid_t pid, int priority) {

    Process* new = (Process*)malloc(sizeof(Process));
    if (!new) {
        perror("unable to initialize new process");
        exit(EXIT_FAILURE);
    }
    new->pid = pid;
    new->priority = priority;
    new->next = NULL;

    if (!ReadyQueue || new->priority < ReadyQueue->priority) {
        // change location based on priority
        new->next = ReadyQueue;
        ReadyQueue = new;
    } else {
        Process* current = ReadyQueue;
        while (current->next && current->next->priority <= new->priority) {
            current = current->next;
        }
        new->next = current->next;
        current->next = new;
    }
    if (!new->next) {
        LastProcess = new;
    }
}


// to check wheather the que is empty or not
bool isEmpty(){
    bool a = (ReadyQueue == NULL);
    return a;
}


//manages process scheduling
void AlarmHandler(int num) {
    printf("Received SIGALRM\n");
    sigset_t mask, prev_mask;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    // Stops and resubmits all running processes to queue
    for (int i = 0; i < RunCount; i++) {
        if (kill(RunningProcesses[i], SIGSTOP) == -1) {
            perror("Unable to stop process");
        }
        Submit(RunningProcesses[i], 1);
    }
    RunCount = 0;

    // Manage the ready queue
    Process *current = ReadyQueue, *prev = NULL;
    int active = 0;

    while (current && active < NCPU) {
        int status;
        int WaitRe = waitpid(current->pid, &status, WNOHANG);

        if (WaitRe == 0) {  // Process is running
            if (kill(current->pid, SIGCONT) == -1) {
                perror("Error in continuing process");
            }
            RunningProcesses[RunCount++] = current->pid;

            // Move the current process based on priority
            if (prev) {
                prev->next = current->next;
            } else {
                ReadyQueue = current->next;
            }

            Process* temp = current;
            current = current->next;

            temp->next = NULL;
            if (LastProcess) {
                LastProcess->next = temp;
                LastProcess = temp;
            } else {
                ReadyQueue = LastProcess = temp;
            }
            active++;
        } else if (WaitRe > 0) {  // Process is finished
            if (prev) {
                prev->next = current->next;
            } else {
                ReadyQueue = current->next;
            }
            free(current);
            if (prev) {
                current = prev->next;
            } else {
                current = ReadyQueue;
            }
        } else {
            perror("waitpid");
            current = current->next;
        }
    }
    
    alarm(TSLICE);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

//handls connections and sets up round-robin scheduler
void HandleConnections() {
    sigset_t mask, old_mask;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    signal(SIGALRM, AlarmHandler);
    alarm(TSLICE);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    sleep(20);  // sleep
}

// Initialize scheduler
void initialise_scheduler(int input_num_cpus, int input_time_slice) {
    NCPU = input_num_cpus;
    TSLICE = input_time_slice;
    signal(SIGALRM, AlarmHandler);
    alarm(TSLICE);
}


// Stops scheduler and frees memory
void stop_scheduler() {
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    while (ReadyQueue) {
        Process *current = ReadyQueue;
        if (waitpid(current->pid, NULL, WNOHANG) == 0) {
            if (kill(current->pid, SIGCONT) == -1) {
                perror("Error in continuing process");
            }
            waitpid(current->pid, NULL, 0);
        } else {
            perror("waitpid error in stop_scheduler");
        }
        ReadyQueue = current->next;
        free(current);
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}
