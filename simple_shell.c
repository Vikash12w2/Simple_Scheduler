#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100

// Structure to hold each command's history
typedef struct CommandHistory {
    char command[MAX_CMD_LEN];
    pid_t pid;
    struct timeval exec_time;
    double duration;
    int priority;  // Add priority field
    struct CommandHistory *next;
} CommandHistory;


// Global variable for command history
CommandHistory *history = NULL;

// Function prototypes
void handle_signal(int signum);
void prompt_user_input(char *input);
void execute_piped_commands(char *input);
void execute_command(char *input);
void parse_and_execute(char *input);
void store_command_history(char *command, pid_t pid, struct timeval start_time, double duration, int priority);
void display_command_history();
void display_execution_details();
int split_piped_commands(char *input, char *commands[], int max_commands);

// Signal handler for termination
void handle_signal(int signum) {
    printf("\nTerminating SimpleShell...\n");
    display_execution_details();  // Show history on exit

    // Free the command history
    CommandHistory *current = history;
    while (current != NULL) {
        CommandHistory *next = current->next;
        free(current);
        current = next;
    }
    exit(EXIT_SUCCESS);  // Exit the shell
}

// Prompt the user for input
//void prompt_user_input(char *input) {
  //  printf("SimpleShell$ ");  // Display custom shell prompt
  //  if (fgets(input, MAX_CMD_LEN, stdin) != NULL) {
    //    // Remove the trailing newline character safely
    //    input[strcspn(input, "\n")] = '\0';  
   // }
//}

//// Execute a command (piped or simple)
//void execute_command(char *input) {
  //  if (input == NULL || input[0] == '\0') {
    //    return;  // Do nothing if the input is empty
   /// }

    //// Check if the command contains a pipe symbol
   // if (strstr(input, "|")) {
     //   execute_piped_commands(input);
   // } else {
     //   parse_and_execute(input);
    //}
//}

// Main function with user interaction loop
int main() {
    char input[MAX_CMD_LEN];

    // Set signal handler for SIGINT (Ctrl-C)
    signal(SIGINT, handle_signal);

    // Shell loop to accept user commands
    while (1) {
        prompt_user_input(input);

        // Handle history display
        if (strcmp(input, "history") == 0) {
            display_command_history();
        }
        else if (strcmp(input, "exit") == 0) {
            handle_signal(SIGINT);  // Clean up and exit
        } else if (strlen(input) > 0) {
            execute_command(input);  // Execute the user command
        }
    }

    // Free command history at the end (unreachable in current logic)
    // But included here for completeness
    CommandHistory *current = history;
    while (current != NULL) {
        CommandHistory *next = current->next;
        free(current);
        current = next;
    }

    return EXIT_SUCCESS;  // Return success status
}

void prompt_user_input(char *input) {
    printf("SimpleShell$ ");  // Display custom shell prompt
    if (fgets(input, MAX_CMD_LEN, stdin) != NULL) {
        // Remove the trailing newline character safely
        input[strcspn(input, "\n")] = '\0';  
    }
}
void execute_command(char *input) {
    if (input == NULL || input[0] == '\0') {
        return;  // Do nothing if the input is empty
    }

    // Check if the command contains a pipe symbol
    if (strstr(input, "|")) {
        execute_piped_commands(input);
    } else {
        parse_and_execute(input);
    }
}





void tokenize_input(char *input, char *args[]) {
    char *token = strtok(input, " ");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {  // Ensure we don't overflow args
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // Null-terminate the argument array
}

void execute_command1(char *args[], int priority) {
    if (strcmp(args[0], "submit") == 0 && args[1] != NULL) {
        execvp(args[1], &args[1]);
        perror("Error executing command");
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "Invalid command. Commands must start with 'submit'\n");
        exit(EXIT_FAILURE);
    }
}



double calculate_duration(struct timeval start_time, struct timeval end_time) {
    return (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
}

void parse_and_execute(char *input) {
    char *args[MAX_ARGS];
    int priority = 1;  // Default priority

    // Tokenize input and check for priority
    tokenize_input(input, args);
    if (args[2] != NULL && strcmp(args[0], "submit") == 0) {
        priority = atoi(args[2]);  // Convert priority to integer
        if (priority < 1 || priority > 4) {
            priority = 1;  // Fallback to default if out of range
        }
    }

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    pid_t pid = fork();
    if (pid == 0) {
        execute_command1(args, priority);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        gettimeofday(&end_time, NULL);

        double duration = calculate_duration(start_time, end_time);
        store_command_history(input, pid, start_time, duration, priority); // Save with priority
    } else {
        perror("Error creating process");
    }
}


void close_pipes(int fd[], int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(fd[i]);
    }
}

void execute_command_with_pipes(char *command, int *fd, int i, int num_commands) {
    if (i > 0) {
        dup2(fd[(i - 1) * 2], STDIN_FILENO);  // Read from the previous pipe
    }
    if (i < num_commands - 1) {
        dup2(fd[i * 2 + 1], STDOUT_FILENO);  // Write to the next pipe
    }

    // Close all pipes after duplicating
    close_pipes(fd, 2 * (num_commands - 1));

    parse_and_execute(command);  // Parse and execute the command
    exit(EXIT_SUCCESS);  // Exit the child process after execution
}

void execute_piped_commands(char *input) {
    char *commands[MAX_ARGS];
    int num_commands = split_piped_commands(input, commands, MAX_ARGS);
    
    if (num_commands < 2) {
        fprintf(stderr, "Error: Please provide at least two commands separated by pipes.\n");
        return;
    }

    int fd[2 * (num_commands - 1)];  // File descriptors for pipes

    // Create pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(fd + i * 2) == -1) {
            perror("Error creating pipe");
            return;
        }
    }

    // Fork processes for each command
    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: set up pipes and execute the command
            execute_command_with_pipes(commands[i], fd, i, num_commands);
        } else if (pid < 0) {
            perror("Error creating process");
            close_pipes(fd, 2 * (num_commands - 1));
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: close all pipes
    close_pipes(fd, 2 * (num_commands - 1));

    // Wait for all child processes to finish
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

int split_piped_commands(char *input, char *commands[], int max_commands) {
    int count = 0;
    char *command = strtok(input, "|");

    while (command != NULL && count < max_commands) {
        commands[count++] = command;  // Store each command
        command = strtok(NULL, "|");
    }

    commands[count] = NULL;
    return count;
}

CommandHistory* create_history_entry(char *command, pid_t pid, struct timeval start_time, double duration) {
    CommandHistory *new_entry = (CommandHistory *)malloc(sizeof(CommandHistory));
    if (new_entry == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    strncpy(new_entry->command, command, MAX_CMD_LEN - 1);
    new_entry->command[MAX_CMD_LEN - 1] = '\0';  // Ensure null termination
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


void print_command_entry(int index, CommandHistory *entry) {
    printf("%d. %s\n", index, entry->command);
}

void display_command_history() {
    CommandHistory *current = history;
    int index = 1;

    while (current != NULL) {
        print_command_entry(index++, current);
        current = current->next;
    }
}

void display_execution_details() {
    CommandHistory *current = history;
    while (current != NULL) {
        double adjusted_duration = current->duration / current->priority;  // Inverse effect: higher priority, shorter time
        printf("Command: %s\n", current->command);
        printf("PID: %d\n", current->pid);
        printf("Priority: %d\n", current->priority);
        printf("Executed at: %ld.%06ld seconds\n", current->exec_time.tv_sec, current->exec_time.tv_usec);
        printf("Adjusted Duration: %.6f seconds\n", adjusted_duration);
        printf("----------------------------\n");
        current = current->next;
    }
}

void cleanup_history(CommandHistory *history) {
    CommandHistory *current = history;
    while (current != NULL) {
        CommandHistory *next = current->next;
        free(current);
        current = next;
    }
}

void signal_handler(int signum) {
    printf("\nTerminating SimpleShell...\n");

    // Display command execution history
    display_execution_details();

    // Cleanup and free command history
    if (history != NULL) {
        cleanup_history(history);
    }

    // Properly terminate the shell
    exit(EXIT_SUCCESS);
}
