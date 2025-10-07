/**
 * @file main.c
 * @brief This is the main file for the shell. It contains the main function that runs the shell.
 * @version 0.1
 * @date 2024-09-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include "string.h"
#include <unistd.h>
#include <sys/wait.h>

struct Alias
{
    char* alias;
    char* command;
};

char** parser(char* input, int* size, struct Alias* aliases); // split a string into words according to spaces
char* handlePwd (); // returns command window as a string
void handleCd (char** command, int size); // changes current directory
char* handleExec (char** command, int size, int* ret); // prints the ls
char* handleAlias (char** command, int numberOfInputs, struct Alias* aliases);
void removeAlias (char* alias, struct Alias* aliases);
void handlePushd(char** command, int size, char** stack);
char* handlePopd(char** stack);
char* handleDirs(char** stack);
char* manageCommand(char* input, struct Alias* aliasList, char** stack, int* ret, int isPipelining);
char** parseBigCommand(char* input, int* numberOfCommands, char** commandType);
char* handleEcho(char** command, int size);

const int BUFFERSIZE = 60000;
char myPreviousCommand[60000];

/**
 * @brief This is the main function for the shell.
 * 
 * @return int 
 */
int main(int argc, char* argv[])
{
    int isInteractive = 1;
    FILE* scriptFile;

    if (argc == 2) {
        // Script file provided, run in script mode
        scriptFile = fopen(argv[1], "r");
        if (!scriptFile) {
            perror("Error opening script file");
            return 1;
        }

        isInteractive = 0;
    }

    struct Alias aliasList[100];
    for (int i = 0; i < 100; i++) {
        aliasList[i].alias = NULL;
        aliasList[i].command = NULL;
    }

    char* stack[100];
    for (int i = 0; i < 100; i++)
        stack[i] = NULL;

    while (1) {
        char* input;

        if (isInteractive) {
            input = readline("$ ");  // Interactive prompt
        } else {
            char buffer[1000];
            if (fgets(buffer, sizeof(buffer), scriptFile) == NULL)
                break;
            input = strdup(buffer);  // Make a copy of the input

            int length = strlen(input);
            if (input[length - 1] == '\n')
                input[length - 1] = '\0';  // Replace the last character with a null terminator
        }

        if (!input)
            break;

        if(strcmp(input, "exit") == 0)
            return 0;

        int numberOfCommands = 1;
        char** commandType = malloc(20 * sizeof(char*));
        char** commands = parseBigCommand(input, &numberOfCommands, commandType);

        for (int toRun = 0; toRun < numberOfCommands; toRun++) {
            int code = 0; // checks status of manageCommand if needed
            int isPipelining = 0; // check if pipelining is true or not

            // check if the command has < > or >> in it
            char* ioType = NULL;
            char* io = NULL;

            for (int i = 1; commands[toRun][i] != '\0'; i++) {
                if (commands[toRun][i-1] == ' ' && (commands[toRun][i] == '<' || commands[toRun][i] == '>')) {
                    ioType = commands[toRun] + i;
                    commands[toRun][i-1] = '\0';
                    if (commands[toRun][i + 1] != ' ') {
                        io = commands[toRun] + i + 3;
                        commands[toRun][i + 2] = '\0';
                    }
                    else {
                        io = commands[toRun] + i + 2;
                        commands[toRun][i + 1] = '\0';
                    }
                    break;
                }
            }
            
            FILE *fptr;
            if (io != NULL) {
                if (strcmp(ioType, "<") == 0) {
                    fptr = fopen(io, "r");

                    if (fptr == NULL) { // don't execute the command
                        fclose(fptr);
                        continue;
                    }

                    int size = fread(myPreviousCommand, 1, BUFFERSIZE, fptr);
                    myPreviousCommand[size] = '\0';

                    // Close the file
                    fclose(fptr);
                }
                else if (strcmp(ioType, ">") == 0) {
                    fptr = fopen(io, "w");
                }
                else if (strcmp(ioType, ">>") == 0) {
                    fptr = fopen(io, "a");
                }
            }

            if (toRun < numberOfCommands - 1 && strcmp(commandType[toRun], "|") == 0) {
                isPipelining = 1;
            }

            char* ret = manageCommand(commands[toRun], aliasList, stack, &code, isPipelining);

            if (io != NULL) {
                if (strcmp(ioType, ">") == 0) {
                    // put ret into the file specified
                    fprintf(fptr, "%s", ret);
                    fclose(fptr);

                    free(ret);
                    ret = NULL;
                }            
                else if (strcmp(ioType, ">>") == 0) {
                    // put ret into the file specified
                    fprintf(fptr, "%s", ret);
                    fclose(fptr);
    
                    free(ret);
                    ret = NULL;
                }
            }

            if (toRun < numberOfCommands - 1 && strcmp(commandType[toRun], "||") == 0) {
                if(code == 0) {
                    toRun++;
                }
            }
            else if (toRun < numberOfCommands - 1 && strcmp(commandType[toRun], "&&") == 0) {
                if(code == 1) {
                    toRun++;
                }
            }

            if (ret != NULL) {
                printf("%s", ret);
                free(ret);
            }
        }

        free(commandType);
        free(commands);
    }

    return 0;
}

short checkIfSpecialChar(char x, char x2) {
    if (x == ';')
        return 1;
    else if (x == '&' || x == '|') {
        if (x2 == '&' || x2 == '|') {
            return 2;
        }
        else {
            return 1;
        }
    }
    else
        return 0;
}

char** parseBigCommand(char* input, int* numberOfCommands, char** commandType) {
    char** commands = malloc(20 * sizeof(char*));

    int start = 0;
    int count = 0;
    for (int i = 1; input[i] != '\0'; i++) {
        if (checkIfSpecialChar(input[i], input[i+1]) != 0 && input[i-1] == ' ') {
            *numberOfCommands = *numberOfCommands + 1;
            commands[count] = input + start;
            commandType[count] = input + i;
            input[i + checkIfSpecialChar(input[i], input[i+1])] = '\0';
            input[i-1] = '\0';
            count++;
            start = i + 1 + checkIfSpecialChar(input[i], input[i+1]);

            i = start - 1;
        }
    }

    commands[count] = input + start;

    return commands;
}

char* manageCommand(char* input, struct Alias* aliasList, char** stack, int* ret, int isPipelining) {
    // logic for parsing input to command
    int numberOfInputs = 1;
    char** command = parser(input, &numberOfInputs, aliasList);

    char* output = NULL;

    // checking which command it is and then redirecting it to handle it
    if(strcmp(command[0], "pwd") == 0) {
        output = handlePwd();
    }
    else if(strcmp(command[0], "cd") == 0)
        handleCd(command, numberOfInputs);
    else if(strcmp(command[0], "echo") == 0) {
        output = handleEcho(command, numberOfInputs);
    }
    else if(strcmp(command[0], "alias") == 0) // need to fix
        output = handleAlias(command, numberOfInputs, aliasList);
    else if(strcmp(command[0], "unalias") == 0) // need to fix
        removeAlias(command[1], aliasList);
    else if(strcmp(command[0], "pushd") == 0)
        handlePushd(command, numberOfInputs, stack);
    else if(strcmp(command[0], "popd") == 0) // need to fix
        output = handlePopd(stack);
    else if(strcmp(command[0], "dirs") == 0) // need to fix
        output = handleDirs(stack);
    else
        output = handleExec(command, numberOfInputs, ret);

    if (isPipelining) {
        for (int i = 0; i < BUFFERSIZE; i++)
            myPreviousCommand[i] = '\0';
        strcpy(myPreviousCommand, output);
        return NULL;
    }
    else {
        myPreviousCommand[0] = '\0';
        return output;
    }
}

void handlePushd(char** command, int size, char** stack) {
    // act like a normal cd command
    // push the current directory into the stack

    char* currentDir = handlePwd();

    if (size == 1) {
        chdir("/");
    }
    // go to said directory
    else {
        chdir(command[1]);
    }

    for (int i = 0; i < 100; i++) {
        if (stack[i] == NULL) {
            stack[i] = currentDir;
            break;
        }
    }
}

char* handlePopd(char** stack) {
    // pop a directory and go to it

    for (int i = 0; i < 100; i++) {
        if (stack[i] == NULL) {
            if (i - 1 >= 0) {
                chdir(stack[i-1]);
                free(stack[i-1]);
                stack[i-1] = NULL;
                break;
            }
            else {
                char* output = malloc(100);
                strcpy(output, "popd: directory stack empty\n\0");
                return output;
            }
        }
    }

    return NULL;
}

char* handleDirs(char** stack) {

    int start = -2;
    for (int i = 0; i < 100; i++) {
        if (stack[i] == NULL) {
            start = i-1;
            break;
        }
    }

    char* output = malloc(BUFFERSIZE);
    int pos = 0;
    for (int i = start; i >= 0; i--) {
        for (int j = 0; stack[i][j] != '\0'; j++) {
            output[pos] = stack[i][j];
            pos++;
        }
        output[pos] = '\n';
        pos++;
    }
    output[pos] = '\0';
    return output;
}

char* handleAlias (char** command, int size, struct Alias* aliases)
{
    // handle a dynamic
    // list for managing aliases

    char* ret = malloc(BUFFERSIZE);
    int pos = 0;

    if (size == 1) {
        for (int i = 0; i < 100; i++) {
            if (aliases[i].alias != NULL) {
                for (int j = 0; aliases[i].alias[j] != '\0'; j++) {
                    ret[pos] = aliases[i].alias[j];
                    pos++;
                }
                ret[pos] = '=';
                pos++;

                ret[pos] = '\'';
                pos++;
                for (int j = 0; aliases[i].command[j] != '\0'; j++) {
                    ret[pos] = aliases[i].command[j];
                    pos++;
                }
                ret[pos] = '\'';
                pos++;
                ret[pos] = '\n';
                pos++;
            }
        }
        ret[pos] = '\0';
        return ret;
    }

    if (size == 2) {
        for (int i = 0; i < 100; i++) {
            if (aliases[i].alias != NULL && strcmp(aliases[i].alias, command[1]) == 0) {
                for (int j = 0; aliases[i].alias[j] != '\0'; j++) {
                    ret[pos] = aliases[i].alias[j];
                    pos++;
                }
                ret[pos] = '=';
                pos++;

                ret[pos] = '\'';
                pos++;
                for (int j = 0; aliases[i].command[j] != '\0'; j++) {
                    ret[pos] = aliases[i].command[j];
                    pos++;
                }
                ret[pos] = '\'';
                pos++;
                ret[pos] = '\n';
                pos++;
            }
        }
        ret[pos] = '\0';
        return ret;
    }

    int toAdd = -1;
    for (int i = 0; i < 100; i++) {
        if (aliases[i].alias == NULL) {
            toAdd = i;
            break;
        }
    }

    if (toAdd == -1)
        return NULL;

    char* ret1 = malloc(128); // Dynamically allocate memory
    strcpy(ret1, command[1]); // copy

    char* ret2 = malloc(128);
    int index = 0;
    for (int i = 2; i < size - 1; i++) {
        for (int j = 0; command[i][j] != '\0'; j++) {
            ret2[index] = command[i][j];
            index++;
        }
        ret2[index] = ' ';
        index++;
    }
    for (int j = 0; command[size - 1][j] != '\0'; j++) {
        ret2[index] = command[size - 1][j];
        index++;
    }
    ret2[index] = '\0';

    aliases[toAdd].alias = ret1;
    aliases[toAdd].command = ret2;

    return NULL;
}

void removeAlias (char* alias, struct Alias* aliases) {

    for (int i = 0; i < 100; i++) {
        if (aliases[i].alias != NULL && strcmp(aliases[i].alias, alias) == 0) {
            free(aliases[i].alias);
            free(aliases[i].command);
            aliases[i].alias = NULL;
            aliases[i].command = NULL;
            break;
        }
    }
}

char* handleExec (char** command, int size, int* ret)
{
    // return the output of the ls command
    int pipefd[2];
    int pipefd2[2];

    pipe(pipefd);
    pipe(pipefd2);

    int check = fork();

    if (check == 0)
    {
        close(pipefd2[1]);
        dup2(pipefd2[0], STDIN_FILENO);
        close(pipefd2[0]);

        // child
        close(pipefd[0]); // close read end of file
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]); // close the write end of the pipe after redirection

        command[size] = NULL;

        if (execvp(command[0], command) == -1) {
            exit(1);
        }

        exit(0);
    }
    else
    {
        // parent
        close(pipefd2[0]);
        if (myPreviousCommand[0] != '\0') {
            write(pipefd2[1], myPreviousCommand, strlen(myPreviousCommand));
        }
        close(pipefd2[1]);

        close(pipefd[1]);

        int status;

        waitpid(check, &status, 0);

        *ret = WEXITSTATUS(status);

        char buffer[BUFFERSIZE];

        int readDone = read(pipefd[0], buffer, BUFFERSIZE - 1);
        close(pipefd[0]);

        buffer[readDone] = '\0';

        char* ret = malloc(readDone + 1);
        strcpy(ret, buffer);

        return ret;
    }
}

void handleCd (char** command, int size)
{
    // go home
    if (size == 1) {
        chdir("/");
    }
    // go to said directory
    else {
        chdir(command[1]);
    }
}

char* handlePwd ()
{
    char word[100];
    getcwd(word, 100);
    char* ret = malloc(100);
    strcpy(ret, word);
    ret[strlen(word)] = '\n';
    ret[strlen(word) + 1] = '\0';
    return ret;
}

char* handleEcho(char** command, int size) {
    char* output = malloc(1000);
    int pos = 0;
    // maybe somehow send that into output
    for (int i = 1; i < size - 1; i++) {
        for (int j = 0; command[i][j] != '\0'; j++) {
            output[pos] = command[i][j];
            pos++;
        }
        output[pos] = ' ';
        pos++;
    }
    for (int j = 0; command[size-1][j] != '\0'; j++) {
        output[pos] = command[size-1][j];
        pos++;
    }
    output[pos] = '\n';
    output[pos+1] = '\0';

    return output;
}

short isQuotation(char* input, int pos) {
    // if there is one ' and at least one ' on the right side, then return true, else return false
    int left1 = 0, left2 = 0;
    for (int i = 0; i < pos; i++) {
        if (input[i] == '\'')
            left1++;
        else if (input[i] == '\"')
            left2++;
    }
    if (left1 == 1 || left2 == 1)
        return 1;
    else
        return 0;
}

char** parser(char* input, int* size, struct Alias* aliases)
{
    char** ret = malloc(20 * sizeof(char*));  // Dynamically allocate memory
    
    int start = 0;
    int i = 0;
    int pos_ret = 0;
    while(input[i] != '\0')
    {
        if (input[i] == ' ' && isQuotation(input + start, i - start) == 0)
        {
            *size = *size + 1;

            input[i] = '\0';
            ret[pos_ret] = input + start;
            pos_ret++;

            // find the next start
            start = i + 1;
        }

        i++;
    }

    ret[pos_ret] = input + start;

    // check for quotations
    for (int i = 0; i < *size; i++) {
        if ((ret[i][0] == '\'' && ret[i][strlen(ret[i]) - 1] == '\'') || (ret[i][0] == '\"' && ret[i][strlen(ret[i]) - 1] == '\"')) {
            ret[i] = ret[i] + 1;
            ret[i][strlen(ret[i]) - 1] = '\0';
        }
    }

    // check for aliases
    for (int i = 0; i < 100; i++) {
        if (aliases[i].alias != NULL && strcmp(aliases[i].alias, ret[0]) == 0) {
            char* newRet = malloc(100);
            strcpy(newRet, aliases[i].command);

            ret[0] = newRet;

            // // What I need is to make sure that when I check for the alias, I also need to parse and change the size according to any spaces inside of it

            int toIncrease = 0;
            for (int j = 0; ret[0][j] != '\0'; j++) {
                if (ret[0][j] == ' ')
                    toIncrease++;
            }

            if (toIncrease > 0) {
                char** ret3 = malloc((*size + toIncrease + 1) * sizeof(char*));

                for (int j = 1; j < *size; j++) {
                    ret3[j + toIncrease] = ret[j];
                }

                *size = *size + toIncrease;

                int count = 0;
                int start = 0;
                for (int j = 0; ret[0][j] != '\0'; j++) {
                    if (ret[0][j] == ' ') {
                        ret[0][j] = '\0';
                        ret3[count] = ret[0] + start;
                        start = j + 1;
                        count++;
                    }
                }
                ret3[count] = ret[0] + start;

                ret = ret3;
            }


            break;
        }
    }

    return ret;
}
