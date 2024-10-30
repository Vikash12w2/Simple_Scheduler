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


// Structure to hold each command's history
typedef struct CommandHistory {
    char command[MAX_INPUT_SIZE];
    pid_t pid;
    struct timeval exec_time;
    double duration;
    int priority;  // Add priority field
    struct CommandHistory *next;
} CommandHistory;


// Global variable for command history
CommandHistory *history = NULL;

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


void shell(){
    printf("\033[H\033[J");
    printf("SIMPLE SHELL\n");
    printf("\n");
    sleep(1);
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
            printf("\nCommand HISTORY of comand without submit\n");
            for (int i = 0; i < history_index; i++) {
                printf("n");
                printf("Command: %s\n", command_history[i].command);
                printf("PID: %d\n", command_history[i].pid);
                printf("Start Time: %ld.%06ld\n", (long)(command_history[i].start_time / CLOCKS_PER_SEC), (long)((command_history[i].start_time % CLOCKS_PER_SEC) * 1000000 / CLOCKS_PER_SEC));
                printf("Execution Time: %.6f seconds\n", command_history[i].duration);
            }

            CommandHistory *current = history;
            while (current != NULL) {
                double adjusted_duration = current->duration / current->priority;  // Inverse effect: higher priority, shorter time
                printf("Command with submit: %s\n", current->command);
                printf("PID: %d\n", current->pid);
                printf("Priority: %d\n", current->priority);
                printf("Executed at: %ld.%06ld seconds\n", current->exec_time.tv_sec, current->exec_time.tv_usec);
                printf("Adjusted Duration: %.6f seconds\n", adjusted_duration);
                printf("----------------------------\n");
                current = current->next;
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

double calculate_duration(struct timeval start_time, struct timeval end_time) {
    return (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
}

CommandHistory* create_history_entry(char *command, pid_t pid, struct timeval start_time, double duration) {
    CommandHistory *new_entry = (CommandHistory *)malloc(sizeof(CommandHistory));
    if (new_entry == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    strncpy(new_entry->command, command, MAX_INPUT_SIZE - 1);
    new_entry->command[MAX_INPUT_SIZE - 1] = '\0';  // Ensure null termination
    new_entry->pid = pid;
    new_entry->exec_time = start_time;
    new_entry->duration = duration;
    new_entry->next = NULL;

    return new_entry;
}


void store_command_history(char *command, pid_t pid, struct timeval start_time, double duration, int priority) {
    CommandHistory *new_entry = create_history_entry(command, pid, start_time, duration);
    if (new_entry == NULL) return;

    new_entry->priority = priority;  // Set priority
    new_entry->next = history;
    history = new_entry;
}

void ParseExecute(char *input) {
    char *args[MAX_INPUT_SIZE];
    int priority = 1;  // Default priority

    char *token = strtok(input, " ");
    int i = 0;
    while (token != NULL && i < MAX_INPUT_SIZE - 1) {  // Ensure we don't overflow args
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // Null-terminate the argument array

    if (args[2] != NULL && strcmp(args[0], "submit") == 0) {
        priority = atoi(args[2]);  // Convert priority to integer
        if (priority < 1 || priority > 4) {
            priority = 1;  // default if out of range
        }
    }

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    pid_t pid = fork();
    if (pid == 0) {

        if (strcmp(args[0], "submit") == 0 && args[1] != NULL) {
        execvp(args[1], &args[1]);
        perror("Error executing command");
        exit(EXIT_FAILURE);
        }

    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        gettimeofday(&end_time, NULL);

        double duration = calculate_duration(start_time, end_time);
        store_command_history(input, pid, start_time, duration, priority); // Save with priority
    } else {
        perror("Error while creating the process");
    }
}


int main(){
    char string[MAX_INPUT_SIZE];
    char *parsed_args[MAX_INPUT_SIZE];
    char*parsed_piped_args[MAX_INPUT_SIZE];
    char a[1000];
    int flag=0;
 

    shell();

    do{
        getcwd(a,sizeof(a));
        printf("\n %s",a);

        if (Command(string)){
            continue;
        }

        if (strcmp(string, "history") == 0) {
            printf("\nCommand HISTORY\n");
            for (int i = 0; i < history_index; i++) {
                printf("n");
                printf("Command: %s\n", command_history[i].command);
                printf("PID: %d\n", command_history[i].pid);
                printf("Start Time: %ld.%06ld\n", (long)(command_history[i].start_time / CLOCKS_PER_SEC), (long)((command_history[i].start_time % CLOCKS_PER_SEC) * 1000000 / CLOCKS_PER_SEC));
                printf("Execution Time: %.6f seconds\n", command_history[i].duration);
            }
        }

        // Check if the input starts with "submit" command
        if (strncmp(string, "submit", 6) == 0) {
            ParseExecute(string);            

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
