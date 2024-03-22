#define _GNU_SOURCE

#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define PORT 33624

void handle_request(int nfd)
{
    FILE *network = fdopen(nfd, "r");
    char *line = NULL;
    size_t size;
    ssize_t num;

    if (network == NULL){
        perror("fdopen");
        close(nfd);
        return;
    }

    while ((num = getline(&line, &size, network)) >= 0){
        char *first_word = strtok(line, " ");
        char *second_word = strtok(NULL, " ");
        
        if (strcmp(first_word, "GET") == 0 && second_word != NULL){
            char *newline_occur = strchr(second_word, '\n');
            if (newline_occur != NULL){
                *newline_occur = '\0'; 
            }
            FILE *file = fopen(second_word, "r");
            if (file == NULL){
                perror("File doesn't exist\n");
                char message[] = "File doesn't exist\n";
                size_t message_size = strlen(message) + 1;
                write(nfd, &message_size, sizeof(message_size));
                write(nfd, message, sizeof(message));
            }
            else {
                fseek(file, 0, SEEK_END);
                long size_of_file = ftell(file);
                fseek(file, 0, SEEK_SET);
                write(nfd, &size_of_file, sizeof(size_of_file));
                char write_buffer[size_of_file];
                size_t elements_read = fread(write_buffer, 1, size_of_file, file);
                write(nfd, write_buffer, elements_read);
                fclose(file);
            }
        }
        else {
            char message[] = "Invalid command\n";
            size_t message_size = strlen(message) + 1;
            write(nfd, &message_size, sizeof(message_size));
            write(nfd, message, sizeof(message));
        }
        free(line);
        line = NULL;
    }
    if (line != NULL){
        free(line);
        line = NULL;
    }
    fclose(network);
}

void sigchild_handler(int signum, siginfo_t *info, void *context){
    printf("Child Process Terminated\n");
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void run_service(int fd){
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = &sigchild_handler;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGCHLD, &action, NULL) < 0){
        perror("sigaction");
    }
    while (1){
        int nfd = accept_connection(fd);
        if (nfd != -1){
            pid_t child;
            if ((child = fork()) < 0){
                perror("fork");
                close(nfd);
            }
            else if (child == 0){
                printf("Connection established\n");
                handle_request(nfd);
                printf("Connection closed\n");
                close(nfd);
                exit(0);
            }
            else{
                close(nfd);
            }
        }
    }
}

int main(void)
{
    int fd = create_service(PORT);
    if (fd == -1)
    {
        perror(0);
        exit(1);
    }

    printf("Listening on port: %d\n", PORT);
    run_service(fd);
    close(fd);

    return 0;
}