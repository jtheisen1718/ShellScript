#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void throw_error(){
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

void handle_pwd(char* command, char* word, char *delim, char *word_ptr){
    word = strtok_r(NULL,delim,&word_ptr);
    if (word == NULL) {
        size_t buff_size = 450;
        char pwd_buffer[buff_size];
        if (getcwd(pwd_buffer,buff_size) == NULL){
            throw_error();
        }
        myPrint(pwd_buffer);
        myPrint("\n");
        return;
    }
    throw_error();
    return;
}

void handle_cd(char* command, char* word, char *delim, char *word_ptr) {
    char* dest = strtok_r(NULL,delim,&word_ptr);
    if (dest == NULL && !chdir(getenv("HOME"))){
        return;
    } else if (!strtok_r(NULL,delim,&word_ptr) && !chdir(dest)) {
        return;
    }
    throw_error();
    return;
}

void handle_command(char* command){
    char delim[] = " \t\n";
    char* redirection_ptr;
    char* instruction = strtok_r(command,">",&redirection_ptr);
    if (instruction == NULL){
        throw_error();
        return;
    }
    char* dest_ptr;
    char* dest;
    char* temp;
    int advanced_redirect = 0;
    int stdo = dup(STDOUT_FILENO);
    int fd;
    int duplicate = -1;
    if ((command[strlen(command) - 1] == '>') || (command[0] == '>')) {
        throw_error();
        return;
    }
    int redirected = (dest = strtok_r(NULL,">",&redirection_ptr)) != NULL;
    
    
    char* word_ptr;
    char* word = strtok_r(instruction,delim,&word_ptr); // Tokenize command
    if (word == NULL){
        if (redirected){
            throw_error();
            return;
        }
        return;
    } else if (!strcmp(word,"exit")){
        if (!redirected && strtok_r(NULL,delim,&word_ptr) == NULL){
            exit(0);
        } else {
            throw_error();
            return;
        }
    } else if (!strcmp(word,"pwd")){
        if (redirected) {
            throw_error();
        } else {
            handle_pwd(command,word,delim,word_ptr);
        }
        return;
    } else if (!strcmp(word,"cd")){
        if (redirected) {
            throw_error();
        } else{
            handle_cd(command,word,delim,word_ptr);
        }
        return;
    }


    if (redirected){
        if (strtok_r(NULL,">",&redirection_ptr) != NULL){ //Multiple redirections
            throw_error();
            return;
        } 
        if (dest[0] == '+'){
            dest++;
            advanced_redirect = 1;
        }
        dest = strtok_r(dest,delim,&dest_ptr);
        if (dest == NULL) { // dest is white space
            throw_error();
            return;
        } else if (!access(dest,F_OK)){ // dest exists
            if (!advanced_redirect){
                throw_error();
                return;
            } else {
                temp = "temporary_file.txt";
                fd = creat(temp,0777);
                if (fd < 0){
                    throw_error();
                    exit(1);
                }
                duplicate = dup2(fd,STDOUT_FILENO);
                if (duplicate < 0){
                    throw_error();
                    exit(1);
                }
            }
        } else if (strtok_r(NULL,delim,&dest_ptr) != NULL){ // multiple dests
            throw_error();
            return;
        } else {
            advanced_redirect = 0;
            fd = creat(dest,0777);
            if (fd < 0){
                throw_error();
                return;
            }
            duplicate = dup2(fd,STDOUT_FILENO);
            if (duplicate < 0){
                throw_error();
                return;
            }
        }
    }

    int args = 0;
    char* arguments[100];
    arguments[args++] = word;
    while ((word = strtok_r(NULL,delim,&word_ptr)) != NULL) {
        arguments[args++] = word;
    }
    arguments[args] = NULL;
    pid_t forkret = fork();
    if (forkret == 0){
        int i;
        if ((i =execvp(arguments[0], arguments)) == -1){
            throw_error();
        }
        exit(0);
    } else {
        waitpid(forkret,NULL,0);
        if (advanced_redirect){
            FILE *destination;
            char *line;
            size_t line_size = 0;
            ssize_t len;

            destination = fopen(dest,"r");
            if (destination == NULL){
                throw_error();
                exit(0);
            }

            while ((len = getline(&line, &line_size,destination)) != -1){
                myPrint(line);
            }

            fclose(destination);

            if (remove(dest) != 0){
                throw_error();
                exit(0);
            }
            if (rename(temp,dest) != 0){
                throw_error();
                exit(0);
            }            
            dup2(stdo,STDOUT_FILENO);
            close(fd);
        } else if(duplicate > 0){
            dup2(stdo,STDOUT_FILENO);
            close(fd);
        }
    }
    return;
}

int main(int argc, char *argv[]) 
{
    if (argc == 2){ // Batch Mode
        FILE *batch;
        char *cmd_buff;
        size_t cmd_buff_size = 0;
        ssize_t len;

        batch = fopen(argv[1],"r");
        if (batch == NULL){
            throw_error();
            exit(0);
        }

        while ((len = getline(&cmd_buff, &cmd_buff_size,batch)) != -1){
            // Handle overflow
            if (len > 514){
                myPrint(cmd_buff);
                throw_error();
                continue;
            }

            char cmd_copy[strlen(cmd_buff)];
            strcpy(cmd_copy,cmd_buff);
            char* white_ptr;
            char* white = strtok_r(cmd_copy," \t\n",&white_ptr);
            if (white != NULL){
                myPrint(cmd_buff);
            }
            // Set up tokenizing
            char* command_ptr;
            char* this_command = strtok_r(cmd_buff,";",&command_ptr);
            

            // Process each command
            while (this_command != NULL){
                handle_command(this_command);
                this_command = strtok_r(NULL, ";",&command_ptr);
            }
        }
        fclose(batch);

    } else if (argc == 1) { // Interactive Mode
        size_t cmd_buff_size = 1500;
        char cmd_buff[cmd_buff_size];
        cmd_buff[513] = 'x';
        char *pinput;
        
        while (1) {
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, cmd_buff_size, stdin);
            
            // Handle read errors
            if (!pinput) {
                throw_error();
                exit(0);
            }
            // Handling overflow
            if (cmd_buff[513] == '\0' && 
                cmd_buff[512] != '\n'){
                myPrint(cmd_buff);
                myPrint("\n");
                throw_error();
                continue;
            }

            // Set up tokenizing
            char* command_ptr = cmd_buff;
            char* this_command = strtok_r(cmd_buff,";",&command_ptr);

            // Process each command
            while (this_command != NULL){
                handle_command(this_command);
                this_command = strtok_r(NULL, ";",&command_ptr);
            }
        }
    } else {
        throw_error();
    }
    exit(0);
}