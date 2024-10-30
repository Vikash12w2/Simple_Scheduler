#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>

// process in the ready queue
typedef struct{
    pid_t pid;
    int ExecutionTime;
    int WaitTime;
    int priority;
    struct Process* next;
} Process;



// Global variables
Process *ReadyQueue = NULL, *last_process = NULL;
int num_cpus, time_slice, running_count = 0;
pid_t running_processes[200];

// Helper function to get PID of a process (currently returns current process PID)
pid_t get_pid_process(Process *p) {
    return p->pid;
}

// Updates the last process pointer in the ready queue
void update_last_process() {
    last_process = ReadyQueue;
    while (last_process && last_process->next != NULL) {
        last_process = last_process->next;
    }
}

// Stops all running processes and resubmits them to the ready queue
void stop_and_resubmit_running_processes() {
    for (int i = 0; i < running_count; i++) {
        if (kill(running_processes[i], SIGSTOP) == -1) {
            perror("Error stopping process");
        }
        submit_process(running_processes[i], 1);
    }
    running_count = 0;
}

// Checks if the ready queue is empty
int is_ReadyQueue_empty() {
    return ReadyQueue == NULL;
}

// Manages the ready queue: resumes processes or removes finished ones
void manage_ReadyQueue() {
    Process *current = ReadyQueue, *prev = NULL;
    int active_count = 0;

    while (current && active_count < num_cpus) {
        int status;
        int wait_res = waitpid(current->pid, &status, WNOHANG);

        if (wait_res == 0) {  // Process is still running
            if (kill(current->pid, SIGCONT) == -1) {
                perror("Error continuing process");
            }
            running_processes[running_count++] = current->pid;

            // Move the current process to the end of the ready queue based on priority
            if (prev) {
                prev->next = current->next;
            } else {
                ReadyQueue = current->next;
            }

            Process* temp = current;
            current = current->next;

            temp->next = NULL;
            if (last_process) {
                last_process->next = temp;
                last_process = temp;
            } else {
                ReadyQueue = last_process = temp;
            }
            active_count++;
        } else if (wait_res > 0) {  // Process finished
            if (prev) {
                prev->next = current->next;
            } else {
                ReadyQueue = current->next;
            }
            free(current);
            current = (prev) ? prev->next : ReadyQueue;
        } else {
            perror("waitpid error");
            current = current->next;
        }
    }
}


// Simulates handling connections and sets up the round-robin scheduler
void handle_connect() {
    sigset_t mask, old_mask;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    signal(SIGALRM, round_robin_alarm_handler);
    alarm(time_slice);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    sleep(20);  // Placeholder sleep
}

// Round-robin alarm handler to manage process scheduling
void round_robin_alarm_handler(int signum) {
    printf("Received SIGALRM\n");
    sigset_t mask, prev_mask;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    stop_and_resubmit_running_processes();
    manage_ReadyQueue();
    alarm(time_slice);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

// Initializes the scheduler with the number of CPUs and time slice
void initialise_scheduler(int input_num_cpus, int input_time_slice) {
    num_cpus = input_num_cpus;
    time_slice = input_time_slice;
    signal(SIGALRM, round_robin_alarm_handler);
    alarm(time_slice);
}

// Creates a new process structure
Process* create_new_process(pid_t pid, int priority) {
    Process* new_process = (Process*)malloc(sizeof(Process));
    if (!new_process) {
        perror("Failed to initialize new process");
        exit(EXIT_FAILURE);
    }
    new_process->pid = pid;
    new_process->priority = priority;
    new_process->next = NULL;
    return new_process;
}

// Submits a process to the scheduler
void submit_process(pid_t pid, int priority) {
    Process* new_process = create_new_process(pid, priority);
    if (!ReadyQueue || new_process->priority < ReadyQueue->priority) {
        // Insert at the start if it has higher priority
        new_process->next = ReadyQueue;
        ReadyQueue = new_process;
    } else {
        // Find the correct position for insertion based on priority
        Process* current = ReadyQueue;
        while (current->next && current->next->priority <= new_process->priority) {
            current = current->next;
        }
        new_process->next = current->next;
        current->next = new_process;
    }
    if (!new_process->next) {
        last_process = new_process;
    }
}


// Stops all processes and frees memory
void stop_scheduler() {
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    while (ReadyQueue) {
        Process *current = ReadyQueue;
        if (waitpid(current->pid, NULL, WNOHANG) == 0) {
            if (kill(current->pid, SIGCONT) == -1) {
                perror("Error continuing process");
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
