/*
 *  File: sh.c
 *  Date:  6 September 2002
 *  Acct: Sarah Filman <sfilman>
 *  Desc:
 * 
 * The overall flow of s-fil shell is this:
 * 1. Get input from user
 * 2. Check if command refers to a built in command
 *    (ln, rm, cd, exit)
 *    a. If it is a built in command, check for errors.
 *       If no errors, execute system call. return to step 1.
 *    b.If it is not a built in command, go to step 3.
 * 3. Parse command line for file redirections. Save these files
 *    and remove references to them in the command line
 * 4. Parse the remainder of the command line for the command the arguments
 * 5. Try to execve the command with associated arguments. Look for
 *    command in hard-coded path locations. Handle errors accordingly.
 * 6. Return to step 1.
 * 
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

//global variable storing the command line input
char command_line[8024];


//function signatures
int checkBuiltIn();
void parseInput(char* envp[]);
void builtIn_cd(char* buff_ptr);
void builtIn_ln(char* buff_ptr);
void builtIn_rm(char* buff_ptr);
int extractPath(char* path_buff, char** buff_ptr);
int extraWord(char* buff_ptr);
int extractWord(char* file_buff, char** buff_ptr);
int execCommand(char* inputFileName, char* outputFileName,
        char* errorFileName, int type_of_output, char* envp[]);
int extractArguments(char** path_buff, char** buff_ptr);
void handleError(int err);


/**
 * The main method is pretty simple-it reads in the input from
 * the user. It calls checkBuiltIn() to see if the user attempted
 * to run a built-In command if that returns true, it skips parsing
 * the input and waits for another input.If the user did not use a
 * builtIn command, the input has to be parsed more carefully
 * for file redirects.
 *TODO: error check read
 */
    int
main(int argc, char *argv[], char *envp[])
{
    printf("s-fil shell\n"); //wait for input from user
    while(1){
        int i;
        for(i = 0; i<8024; i++){
            command_line[i] = '\0'; 
        }

        printf("$ ");
        fflush(NULL); //make sure all output is shown
        int n = 0;
        if((n = read(0, command_line, 8024)) == -1){
            printf("sh: i/o error\n"); 
        }


        if(n == 0){ //user hit control-D with no input
            printf("\n");
            exit(0); //exit shell
        }
        char* newline = strchr(command_line, '\n');
        if(newline == NULL){//user hit control-D, no newline. 
            printf("\n");
            *(command_line + n) = '\n';

        }
        if(checkBuiltIn()) continue;

        //if not a built in, then we will parse the command line 
        //for redirects and execve the contents
        parseInput(envp);
    }

    return 0;
}

/**
 * checkBuiltIn() is used parse the command line to check for builtIn
 * commands. Because we don't worry about file redirects with builtIn
 * commands, the first word we find should correspond to either cd,
 * ln, rm, or exit. Once it finds that word, it calls the subroutine
 * of the corresponding command so it can execute the system call
 * and check for errors in syntax return values: 1 = was builtin command
 * 0 = need to parse line (i.e. not builtin command)
 */
int
checkBuiltIn(){

    //check if buffer is empty
    int charfound = 0;
    char* buff_ptr = command_line;
    while(*buff_ptr != '\n'){
        if(isspace(*buff_ptr) == 0){ charfound = 1;}
        buff_ptr++;
    }
    if(charfound == 0){
        printf("Invalid null command.\n");
        return 1; //return to waiting for new user input
    }
    //end check empty buffer


    //extract command
    char command[256]; //store command
    char* command_ptr = command;
    buff_ptr = command_line;

    command_line[255] = '\0';
    //iterate through buffer to isolate command
    int char_found = 0; 
    while(*buff_ptr != '\0'){
        if(isspace(*buff_ptr) == 0){ //if not whitespace, found word
            if(*buff_ptr == '\n'){
                break;
            };
            *command_ptr = *buff_ptr; //set letter in command to this letter
            command_ptr++; //update pointer
            char_found = 1; //we have started word
        }
        else{ //else we are looking at whitespace. Is this before the
            //command or after?
            if(char_found == 1){ //if char_found == 1,
                //we have already encountered a word 
                *command_ptr = '\0'; //end string in command buffer
                *(command_ptr+1) = '\0'; 
                break;
            }
        }
        buff_ptr++;

    }
    //When we reach the end of the while loop, we should have
    // the command without spaces in the command buffer
    /*Check if it is a built in command*/
    //change directory
    if(strcmp(command, "cd") == 0){
        builtIn_cd(buff_ptr);
        return 1;
    }
    //hard-link
    else if (strcmp(command, "ln") == 0){
        builtIn_ln(buff_ptr);
        return 1;
    }
    //exit
    else if (strcmp(command, "exit") == 0){
        exit(0); //TODO: cleanup handler to close open fd?
    }
    //rm
    else if (strcmp(command, "rm") == 0){
        builtIn_rm(buff_ptr);
        return 1;
    }
    else{
        return 0;
    }
    return 0;

}

/**
 * builtIn_cd
 *
 */
void
builtIn_cd(char* buff_ptr){
    //get path from buffer
    char path[256];
    int wordExist = extractPath(path, &buff_ptr);
    if(!wordExist){
        printf("cd: too few arguments\n");
        return;
    }

    //check for too many arguments
    if(extraWord(buff_ptr) == 1){
        printf("cd: too many arguments\n");
        return;
    }
    //DO ERROR CHECKING!
    if(chdir(path) == -1){
        if(errno == EACCES){
            printf("cd: Permission denied\n");
        }
        else if((errno == ENOTDIR) | (errno == EBADF) |
                (errno == ENOENT)){
            printf("cd: No such file or directory\n");
        }
        else printf("cd: %d\n", errno);

    }

}

/**
 * builtIn_ln
 *
 */
void
builtIn_ln(char* buff_ptr){
    //get old path
    char oldpath[256];
    int wordExist = extractPath(oldpath, &buff_ptr);
    if(!wordExist){
        printf("ln: too few arguments\n");
        return;
    }
    //get new path
    char newpath[256];
    wordExist = extractPath(newpath, &buff_ptr);
    if(!wordExist){
        printf("ln: too few arguments\n");
        return;
    }

    //check if any other text, error if there is.

    if(extraWord(buff_ptr) == 1){
        printf("ln: too many arguments\n");
        return;
    }

    //DO ERROR CHECKING!
    if(link(oldpath, newpath) == -1){
        if((errno == EACCES)|(errno == EFAULT)) {
            printf("ln: Permission denied\n");
        }
        else if((errno == EEXIST)) {
            printf("ln: New path already exists\n");
        }
        else if((errno == ENOENT) | (errno == ENOTDIR)) {
            printf("ln: Path does not exist\n");
        }
        else if((errno == ENAMETOOLONG)) {
            printf("ln: Path too long\n");
        }
        else if((errno == EMLINK)) {
            printf("ln: Maximum number of links exceeded\n");
        }
        else if((errno == EPERM)) {
            printf("ln: Old path is a directory\n");
        }
        else if((errno == EXDEV)) {
            printf("ln: Cannot cross-device link\n");
        }
        else printf("ln: %d\n", errno);
    }
}

/**
 * builtIn_rm
 * int unlink(const char *pathname);
 */

void
builtIn_rm(char* buff_ptr){
    int num = 0;
    //get path from buffer
    while(1){
        char path[256];
        int moreWords = extractPath(path, &buff_ptr);
        //printf("%s\n", path);
        if(!moreWords){
            if(num == 0){
                printf("rm: Too few arguments\n");
                break;
            }
            break;
        }
        num = 1;

        //try to unlink that path. Handle errors accordingly
        if(unlink(path) == -1){
            if((errno == EACCES)|(errno == EPERM)){
                printf("rm: Permission denied\n");
                //continue;
            }
            else if((errno == ENOTDIR) | (errno == EFAULT) |
                    (errno == ENOENT)){
                printf("rm: No such file or directory, %s\n", path);
                //continue;
            }

            else if(errno == EISDIR){
                printf("rm: Is a directory\n"); 
                //continue;
            }
            else{ printf("rm: %d\n", errno);
                //continue;
            }

        }
    }

}

/**
 * extractPath
 *
 */
int
extractPath(char* path_buff, char** buff_ptr){
    int charfound = 0;
    while(**buff_ptr != '\0'){
        if(isspace(**buff_ptr) == 0){
            if(**buff_ptr == '\n'){
                break;
            }
            charfound = 1;
            *path_buff = **buff_ptr;
            path_buff++;

        }

        else{ //this is whitespace
            if(charfound == 1){
                *path_buff = '\0'; //end string in command buffer
                break;
            }
        }
        (*buff_ptr)++;
    }
    return charfound;
}


/**
 * extractArguments
 *
 */
int
extractArguments(char** path_buff, char** buff_ptr){
    int charfound = 0;
    while(**buff_ptr != '\0'){
        if(isspace(**buff_ptr) == 0){
            if(**buff_ptr == '\n'){
                break;
            }
            charfound = 1;
            **path_buff = **buff_ptr;
            (*path_buff)++;

        }
        else{ //this is whitespace
            if(charfound == 1){
                **path_buff = '\0'; //end string in command buffer
                break;
            }
        }
        (*buff_ptr)++;
    }
    return charfound;
}


/**
 * extraWord - used to see if there are too many arguments in cd or ln
 * 0 = false 1 = true;
 */
int
extraWord(char* buff_ptr){
    //buff_ptr++;
    while((*buff_ptr) != '\n'){
        if(isspace(*buff_ptr) == 0){
            return 1;
        }
        buff_ptr++;
    }
    return 0;

}

/**
 *parseInput is called if the user didn't select a builtIn command. It checks
 *for file redirects, saves those files, replaces file redirect references with
 *spaces, and calls execCommand().
 */
void parseInput(char *envp[]) {
    char inputFileName[256] = "/dev/tty";
    char outputFileName[256] = "/dev/tty";
    char errorFileName[256] = "/dev/tty";

    /**Check for correctly formatted redirects**/
    int input_redirect = 0; //flag if we've seen input redirect
    int output_redirect = 0; //flag if we've seen output redirect
    int type_of_output = 0; //1 for >. 2 for >>
    char* command_buff = command_line;
    size_t strleng =  strlen(command_buff);
    strleng--;
    int i;
    for(i = 0; i< strleng; i++){
        char curr_char = *command_buff;
        if(curr_char == '<'){
            if(input_redirect != 0){
                printf("error: Multiple redirects\n");
                return;
            }

            input_redirect = 1;
        }

        //if find first >, check if multiple redirects
        else if(curr_char == '>'){ 
            if(output_redirect != 0){
                printf("error: Multiple redirects\n");
                return;
            }
            //register that we found one, and check if it has a second >
            //if it does, position the buffer pointer after the second >
            output_redirect = 1;
            if((*(command_buff + 1)) == '>') {
                type_of_output = 2;
                command_buff = command_buff + 2; //skip over second >
                i = i + 1;
                continue;
            }
            else{
                type_of_output = 1; 
            }


        }
        else {}//printf("found regular character\n");}
        command_buff++;
}
/**End of checking for correctly formatted redirects**/

//if we've made it here, then there may be redirects, but
//they are formatted correctly. i.e. there aren't multiple
/**extract file name is there is a redirect**/
command_buff = command_line; //reset pointer to beginning of buffer
if(input_redirect){
    char* input_char = strchr(command_buff, '<');
    char inputFile[256];
    int fileExist = extractWord(inputFile, &input_char);
    if(fileExist){
        strcpy(inputFileName,inputFile);;
    }
    else{
        printf("sh: expected filename after i/o redirect\n"); return;
    }

    /**remove reference to file redirects**/
    size_t len = strlen(inputFile);
    int j;
    //starting with the < character, find the location of the file
    //name, and set each character to a space
    input_char = strchr(command_buff, '<');
    char* fileInBuff = strstr(input_char, inputFile);
    if(fileInBuff == NULL){ printf("NULL\n");}
    *input_char = ' ';
    for(j = 0; j< len; j++){
        *fileInBuff = ' ';
        fileInBuff++;
    }

}
char outputFile[256];
if(output_redirect == 1){

    if(type_of_output == 1){
        char* output_char = strchr(command_buff, '>');
        int fileExist2 = extractWord(outputFile, &output_char);
        if(fileExist2){
            strcpy(outputFileName,outputFile);
        }
        else{
            printf("sh: expected filename after i/o redirect\n");
            return;
        }

        /**remove reference to file redirects**/
        size_t len = strlen(outputFile);
        int j;
        //starting with the < character, find the location of the file
        //name, and set each character to a space
        output_char = strchr(command_buff, '>');
        char* fileInBuff = strstr(output_char, outputFile);
        if(fileInBuff == NULL){ printf("NULL\n");}
        *output_char = ' ';
        for(j = 0; j< len; j++){
            *fileInBuff = ' ';
            fileInBuff++;
        }
    }

    else{ //>>
        char* output_char = strstr(command_buff, ">>");
        output_char++;
        int fileExist2 = extractWord(outputFile, &output_char);
        if(fileExist2){
            //printf("output file name1: %s\n",outputFile);
            strcpy(outputFileName,outputFile);
        }
        else{
            printf("sh: expected filename after i/o redirect\n");
            return;
        }
        /**remove reference to file redirects**/
        size_t len = strlen(outputFile);
        int j;
        //starting with the < character, find the location of the file
        //name, and set each character to a space
        output_char = strchr(command_buff, '>');
        char* fileInBuff = strstr(output_char, outputFile);
        if(fileInBuff == NULL){ printf("NULL\n");}

        *output_char = ' ';
        output_char++;
        *output_char = ' ';

        for(j = 0; j< len; j++){
            *fileInBuff = ' ';
            fileInBuff++;
        }
    }
}

execCommand(inputFileName, outputFileName, errorFileName,
        type_of_output, envp);
}

/**
 *At this point, we have gotten rid of all file redirect references,
 *and are ready to take the command and exec it.We first get the command
 *and then arguments. We close file descriptors 0,1,and 2, and if there
 *are file redirects, open those files. The default files are "/dev/tty".
 *There are hard coded paths that execve tries before it fails.
 */
int
execCommand(char* inputFileName, char* outputFileName, char* errorFileName, int type_of_output, char* envp[]){


    //get command and then arguments
    char command[256];
    char arguments[256];
    char* buff_ptr = command_line;
    int command_exist = extractPath(command, &buff_ptr);
    if(!command_exist){
        printf("Invalid null command.\n");
        return 0;
    }
    char* arg_ptr = arguments;
    //note: may need to not have space at end of arguments
    char* argvarray[50];
    argvarray[0] = "";
    int i = 1;
    while(command_exist){
        char* arg_ptr_saved = arg_ptr;
        command_exist = extractArguments(&arg_ptr, &buff_ptr);
        if(command_exist){
            argvarray[i] = arg_ptr_saved;
            i++;
            *arg_ptr = '\0';
            arg_ptr++;
        }

    }
    argvarray[i] = NULL;
    *arg_ptr = '\0';
    //end extracting command

    int kid_pid;
    if((kid_pid = fork()) == 0){

        if(close(0)==-1) { printf("sh: error %d", errno); return 0;}

        if(open(inputFileName, O_RDONLY) == -1){
            int err = errno; handleError(err); return 0;
        }

        if(close(1)==-1) {fprintf(stderr, "sh: error %d \n", errno);  return 0;}

        if(type_of_output == 1){  
            if(open(outputFileName, O_CREAT|O_TRUNC|O_WRONLY,0666) == -1) {
                fprintf(stderr, "sh: error %d \n", errno);
                return 0;
            }
        }
        else if(type_of_output == 2){
            if(open(outputFileName, O_CREAT|O_APPEND|O_WRONLY,0666) == -1){
                fprintf(stderr, "sh: error %d \n", errno);
                return 0;

            }
        }

        else{ //no file redirects
            if(open(outputFileName, O_WRONLY,0666) == -1){
                fprintf(stderr, "sh: error %d \n", errno);
                return 0;

            }       
        }

        //hard code paths to search for command.
        execve(command, argvarray, envp);
        char path1[1024] = "/bin/";
        execve(strcat(path1, command), argvarray, envp);
        char path2[1024] = "/usr/bin/";
        execve(strcat(path2, command), argvarray, envp);
        char path3[1024] = "/local/bin/";
        execve(strcat(path3, command), argvarray, envp);
        char path4[1024] = "/usr/local/bin/";
        execve(strcat(path4, command), argvarray, envp);

        if(errno == ENOENT){
            fprintf(stderr, "sh: command not found: %s \n", command);
            exit(1);
        }
        else{
            fprintf(stderr, "sh: execution of %s failed: %s\n",command, strerror(errno));
            exit(1);
        }

    }
    int status;
    waitpid(kid_pid, &status, 0); 
    return 1;
}

//extractWord find the files after a redirect character
int
extractWord(char* fileName_buff, char** buff_ptr){

    int charfound = 0;
    while(**buff_ptr != '\0'){
        if((isspace(**buff_ptr) == 0) && (**buff_ptr != '<') &&
                (**buff_ptr != '>')) {
            if(**buff_ptr == '\n'){
                break;
            }
            charfound = 1;
            *fileName_buff = **buff_ptr;
            fileName_buff++;

        }

        else{ //this is whitespace or redirect character
            if(charfound == 1){
                *fileName_buff = '\0'; //end string in command buffer
                break;
            }
        }
        (*buff_ptr)++;
    }
    return charfound;
}

//handleError abstracts out the error testing 
void
handleError(int err){

    if((err == EACCES)|(err == EFAULT)) {
        printf("sh: Permission denied\n");
    }
    else if((err == EEXIST)) {
        printf("sh: path already exists\n");
    }
    else if((err == EISDIR)) {
        printf("sh: pathname is directory\n");
    }

    else if((err == ENAMETOOLONG)) {
        printf("sh: Path too long\n");
    }
    else if((err == ENOENT)) {
        printf("sh: File does not exist\n");
    }

    else{ printf("sh: %d\n", err);}
}




