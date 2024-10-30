#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include <sys/time.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<sys/stat.h>
#include<errno.h>
#include<fcntl.h>
#include<signal.h>
#include<stdbool.h>

#define MAX_INPUT_SIZE 1000
#define MAX_HISTORY_SIZE 1000
#define MAX_QUEUE_SIZE 1000

int NCPU,TSLICE;
int pid_scheduler;

typedef struct{
    int pid;
    int ExecutionTime;
    int WaitTime;
} Process;

typedef struct {
    Process comands[MAX_QUEUE_SIZE];
    int front, rear;
} ReadyQueue;

typedef struct {
    char command[MAX_INPUT_SIZE];
    pid_t pid;
    clock_t start_time, end_time;
    double duration;
} command_info;

command_info command_history[MAX_INPUT_SIZE];
int history_index = 0;

void initialize_files() {
    // Wait for system_parameter file to be created by scheduler
    int max_retries = 5;
    int retries = 0;
    int fd;
    
    while (retries < max_retries) {
        fd = open("system_parameter", O_RDONLY);
        if (fd != -1) {
            close(fd);
            break;
        }
        sleep(1);
        retries++;
    }
    
    if (retries == max_retries) {
        printf("Error: Scheduler not running. Please start the scheduler first.\n");
        exit(1);
    }
}

void shell(){
    printf("\033[H\033[J");
    printf("SIMPLE SHELL\n");
    printf("\n");
    sleep(1);
}

bool isEmpty(ReadyQueue *q){
    bool a = q->front == -1;
    return a;
}

Process dequeue(ReadyQueue *q)
{
    if (isEmpty(q)){
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

int Command(char *string) {
    char *buffer;

    buffer = readline("$"); // Use readline for user input
    if (buffer == NULL) {
        return 1; // Handle EOF
    }
    
    // Add the input to the command history
    if (strlen(buffer) > 0) {
        add_history(buffer);
    }
    
    // Copy the input to str and remove the trailing newline
    strcpy(string, buffer);
    free(buffer); // Free the buffer allocated by readline
    return 0;
}

int create_new_process(){
    pid_t pid = fork();
    return pid;
}

void exec_args(char** parsed) {
    
    clock_t start_time, end_time;


    start_time = clock();
    pid_t status = create_new_process();
    end_time = clock();

    double duration = (end_time - start_time);


    switch(status){
        case -1:
            perror("fork");
            break;

        case 0:
            if (execvp(parsed[0], parsed) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            break;

        default:
            int parent_status;
            pid_t wpid = waitpid(status, &parent_status, 0);
            if (wpid == -1) {
                perror("waitpid");
            } 
            else {
                if (WIFEXITED(parent_status)){

                        if (history_index < MAX_HISTORY_SIZE) {
                            strncpy(command_history[history_index].command, parsed[0], MAX_INPUT_SIZE);
                            command_history[history_index].pid = status;
                            command_history[history_index].start_time = start_time;
                            command_history[history_index].end_time = end_time;
                            command_history[history_index].duration = duration;
                            history_index++;
                        } 
                        else {
                            printf("History is full\n");
                        }
                }
            }
            break;
    }
}


void exec_piped_args(char** parsed, char** parsed_pipe) {
    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t pipe1, pipe2;
    clock_t start_time1, end_time1;


    if (pipe(pipefd) < 0) {
        perror("Could not initialized pipe");
        return;
    }

    start_time1 = clock();
    pipe1 = create_new_process();
    end_time1 = clock();
    double duration1 = (end_time1 - start_time1);
    switch(pipe1){
        case -1: 
            perror("could not fork");
            return;

        case 0:
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
    
            if (execvp(parsed[0], parsed) < 0) {
                perror("execvp failed for comand 1");
                exit(EXIT_FAILURE);
            }
            break;
        
        default:
            clock_t start_time2, end_time2;
            start_time2 = clock();
            pipe2 = create_new_process();
            end_time2 = clock();
            double duration2 = (end_time2 - start_time2);
    
            switch(pipe2){
                case -1:
                    perror("Could not fork");
                    return;

                case 0:
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
        
                    if (execvp(parsed_pipe[0], parsed_pipe) < 0) {
                        perror("execvp failed for comand 2");
                        exit(EXIT_FAILURE);
                    }

                default:
                    close(pipefd[0]);  
                    close(pipefd[1]);
        
                    int parent_status1, parent_status2;

                    pid_t wpid1 = (pipe1, &parent_status1, 0);

                    if (wpid1 == -1) {
                        perror("waitpid 1 = -1");
                    }
                    else {
                        if (WIFEXITED(parent_status1)) {
                            if (history_index < MAX_HISTORY_SIZE) {
                                strncpy(command_history[history_index].command, parsed[0], MAX_INPUT_SIZE);
                                command_history[history_index].pid = pipe1;
                                command_history[history_index].start_time = start_time1;
                                command_history[history_index].duration = duration1;
                                history_index++;
                            }
                            else {
                                printf("History is full\n");
                            }
                        }
                    }

                    pid_t wpid2 = waitpid(pipe2, &parent_status2, 0);

                    if (wpid2 == -1) {
                        perror("waitpid 2 = -1");
                    } 
                    else {

                        if (WIFEXITED(parent_status2)) {
                            if (history_index < MAX_HISTORY_SIZE) {
                                strncpy(command_history[history_index].command, parsed_pipe[0], MAX_INPUT_SIZE);
                                command_history[history_index].pid = pipe2;
                                command_history[history_index].start_time = start_time2;
                                command_history[history_index].duration = duration2;
                                history_index++;
                            }
                            else {
                                printf("History is full\n");
                            }
                        }
                    }
            }
    }
}

int command_handler(char** parsed) {
    int i, switch_arg = 0;
    char* list_Of_commands[2];
    char* username;

    list_Of_commands[0] = "cd";
    list_Of_commands[1] = "exit";

    for (i = 0; i < 2; i++) {
        if (strcmp(parsed[0], list_Of_commands[i]) == 0) {
            switch_arg = i + 1;
            break;
        }
    }

    switch (switch_arg) {
        case 1:
            if (parsed[1] == NULL) {
                char* home = getenv("HOME");
                if (home != NULL) {
                    chdir(home);
                } else {
                    perror("HOME environment variable not set.\n");
                }
            }
            else {
                if (chdir(parsed[1]) != 0) {
                    perror("cd");
                }
            }
            return 1;
        case 2:
            printf("\nCommand HISTORY\n");
            for (int i = 0; i < history_index; i++) {
                printf("n");
                printf("Command: %s\n", command_history[i].command);
                printf("PID: %d\n", command_history[i].pid);
                printf("Start Time: %ld.%06ld\n", (long)(command_history[i].start_time / CLOCKS_PER_SEC), (long)((command_history[i].start_time % CLOCKS_PER_SEC) * 1000000 / CLOCKS_PER_SEC));
                printf("Execution Time: %.6f seconds\n", command_history[i].duration);
            }

            if (access("final_information", F_OK) != -1) {
        int fd = open("final_information", O_RDONLY);
        if (fd == -1) {
            perror("Error while opening final_information");
        }
            else{
            int size;
            ReadyQueue queue;
            ssize_t read_size = read(fd, &size, sizeof(int));
            if (read_size == -1) {
                perror("Error while reading queue size");
            } else if (read_size == 0) {
                printf("No scheduled processes information available\n");
            } else {
                // Initialize queue
                queue.front = 0;
                queue.rear = size - 1;
                
                if (read(fd, &queue, sizeof(ReadyQueue)) != -1) {
                    printf("\nScheduled processes information:\n");
                    for (int i = 0; i < size && !isEmpty(&queue); i++) {
                        Process process = dequeue(&queue);
                        printf("\nPID: %d\n", process.pid);
                        printf("Execution time: %d ms\n", process.ExecutionTime);
                        printf("Wait time: %d ms\n", process.WaitTime);
                    }
                } else {
                    perror("Error while reading queue data");
                }
            }
            close(fd);
        }
    } else {
        printf("\nNo scheduling information available yet\n");
    }

    printf("\nShell Closed\n");
    exit(0);
            
        default:
            break;
    }

    return 0;
}

int parse_pipe(char* string, char** string_piped)
{
    
    int pipe_count = 0;
    int a = 0;
    while (string[a] != '\0') {
        if (string[a] == '|') {
            pipe_count++;
        }
        a++;
    }
    
	int i = 0;
    
	while (i < pipe_count+1) {
		string_piped[i] = strsep(&string, "|");
		if (string_piped[i] == NULL){
			break;
        }
        i++;
	}

	if (pipe_count == 0)
		return 0; // returns zero if no pipe is found.
	else {
		return 1;
	}
}

void parse_space(char* string, char** parsed){
    int i = 0;
	while ( i < MAX_INPUT_SIZE) {
		parsed[i] = strsep(&string, " ");

		if (parsed[i] == NULL){
			break;
        }

		if (strlen(parsed[i]) == 0){
			continue;
        }

        i++;
	}
}


int string_read(char* string, char** parsed, char** parsed_pipe){

	char* string_piped[2];
	int pipe = 0;

	pipe = parse_pipe(string, string_piped);

	if (pipe) {
		parse_space(string_piped[0], parsed);
		parse_space(string_piped[1], parsed_pipe);

	} else {

		parse_space(string, parsed);
	}

	return command_handler(parsed) ? 0 : 1 + pipe;
}

void runInBackground(char* comand) {

    pid_t status = create_new_process();

    if (status == -1){
        perror("fork");
        return;
    }
    else if (status == 0){
        // Detach from the shell's controlling terminal
        setsid();
        // Redirect standard input, output, and error to /dev/null
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        // Execute the program
        char *args[2];
        args[0] = comand;
        args[1] = NULL;
        if (execvp(comand, args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }
    else{
        kill(status,SIGSTOP);
        int fd = open("pid_processes", O_WRONLY | O_APPEND);
        if (fd == -1) {
            // If file doesn't exist, create it
            fd = open("pid_processes", O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("Error opening process_pids file");
                exit(1);
            }
        }
        ssize_t write_result = write(fd, &status, sizeof(int));
        if (write_result == -1) {
            perror("Error writing to process_pids file");
            close(fd);
            exit(1);
        }
        close(fd);

        if (kill(pid_scheduler, SIGCONT) == -1) {
            perror("Error signaling scheduler");
        }
        
        printf("Process %d submitted for scheduling\n", status);
    }
}


int main(){
    initialize_files();
    char string[MAX_INPUT_SIZE];
    char *parsed_args[MAX_INPUT_SIZE];
    char*parsed_piped_args[MAX_INPUT_SIZE];
    char a[1000];
    int flag=0;

    shell();

    printf("Enter total number of CPU resources: ");
    scanf("%d",&NCPU);
    printf("Enter the time quantum(in milliseconds) permitted to execute on a core: ");
    scanf("%d",&TSLICE);

    int fd1 = open("system_parameter",O_RDWR);
    if(fd1 == -1){
        printf("Error while reading system parameter\n");
        exit(0);
    }
    if (read(fd1, &pid_scheduler, sizeof(int)) == -1) {
        perror("Error reading scheduler PID");
        close(fd1);
        exit(1);
    }
    printf("pid_scheduler %d\n",pid_scheduler);

    
    if (write(fd1, &NCPU, sizeof(int)) == -1 || write(fd1, &TSLICE, sizeof(int)) == -1) {
        perror("Error writing parameters");
        close(fd1);
        exit(1);
    }
    close(fd1);
     printf("Successfully initialized. Scheduler PID: %d\n", pid_scheduler);

    do{
        getcwd(a,sizeof(a));
        printf("\n %s",a);

        if (Command(string)){
            continue;
        }

        // Check if the input starts with "submit" command
        if (strncmp(string, "submit", 6) == 0) {
            // Extract the program name from the input
            char* comand = string + 7;
            // Run the program in the background
            runInBackground(comand);

        } else {
            flag = string_read(string,parsed_args,parsed_piped_args);
    
		    if (flag == 1){
		    	exec_args(parsed_args);
            }
    
		    if (flag == 2){
		    	exec_piped_args(parsed_args, parsed_piped_args);
            }
        }
        
    } while (1);
    
	return 0;
}
