//#-------------------INCLUDES----------------------#//
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
int got_signal = 0;
pid_t childid=-1;
void sigtstpHandler(int sig)
        {
            got_signal=1;
            /*if(childid>0)
            {
                kill(SIGTSTP,childid);
            }*/
        }
void printDir()
{ // Prints the current directory working in.
    struct passwd *pwd;
    pwd = getpwuid(getuid());
    long size = pathconf(".", _PC_PATH_MAX);
    char buffer[100];
    if (buffer != NULL)
    {
        printf("%s@%s>", pwd->pw_name, getcwd(buffer, (size_t)size));
    }
}
//==========================SIGNAL HANDLERS==========================================================================================================//

int main(int argc, char *argv[])
{
      
//============================ARRAYS===========================================================================================//    
    char* command;     // The first array to hold the input from the user.
    char **to_exec;    // The array that will hold all the arguments to execute.
    char *temp_cmd;    // Temp cmd changes according to the terms needed, will hold mutations of the original cmd.
    char **exec_right; // If there is a pipe, this will hold all the arguments before the pipe operator.
    char **exec_left;  // If there is a pipe, this will hold all the argument after the pipe operator.
    char **new_exec;   // If there's a redirection operaotr, this will hold all the arguments without the redirection operator and the file name following it.
    char* safe_copy;   // A safe copy of the original string. 
    char* temp_copy;    // A temp copy to manipulate on.
    char* argument;    // This will hold the argument cut from the command using strtok().
//============================COUNTERS===========================================================================================//    
    int command_count=0;   // A counter for how many commands there are in general.
    int commands_length=0; // A counter to sum the length of each command and sum them together.
    int commands__pipe=0; // A counter for how many commands include pipe operator.
    int commands_redirection=0; // A counter for how many commands include a redirection operator.
    int bytes_from_pipe;    // A counter for how many bytes are read frin the pipe with read.
//===========================FLAGS================================================================================================//    
    int to_direct;        // An indicator if noticed a special operator to redirect input/output to a file etc..
    int direction;        // The direction. 1 == '>' , 2 == '<', 3== '>>' , 4 == '2>' 
    int to_pipe;          // An indicator if noticed a pipe operator.
    int running=1;        // An indicator that will change to ==0 if 'done' was entered in the cmd.
    
//===========================INDEXES================================================================================================//    
    int cutting_index;   // A var to hold the cutting index of the whole cmd if has a redirection.
    int pipe_index;      // A var to hold the index of the pipe operator.
    int index;           // Index, helps to stay in the boundries.
    int temp_indx;       // Temp index for further use.
    int last_letter;     // A var to hold the last letter in the cmd.
    int direction_index; // A var to hold the index of the special operator if needed.
    int temp_cmd_size;   // A var to hold the size of the temp cmd array.
    int to_exec_size;    // A var to save the size of to_exec.
    int argument_size;  // A var to save the length of argument.
    signal(SIGTSTP, sigtstpHandler);
//===========================FILE DESCRIPTORS================================================================================================// 
   
    int pipe_fd[2];     // Pipe file descriptors.
    int fd;             // File descriptor for requested files from the cmd.
    int log_fd;         // Log.txt file descriptor.
    if(argc > 1) // Open log.txt if given as an argument.
    {
        log_fd = open(argv[1], O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
    }


//==========================SIGNAL HANDLING========================================================================================================//


//====================================================================================================================================================================//    
    while (running == 1)
    {
//===========================INITIALIZE VARIABLES================================================================================================//    
        cutting_index = -1;
        last_letter = -1;
        pipe_index = -1;
        pipe_fd[2];
        to_pipe = 0;            
        to_direct = 0;         
        direction = 0;        
        direction_index = -1;
        if (pipe(pipe_fd) == -1)
        {
            perror("Cannot open pipe");
            exit(EXIT_FAILURE);
        }
        command_count++;
        printDir(); //Print the shell command line showing the user and the current directory ("user@path>").
        int space_count = 0;                                // Count how many spaces to know how many arguments does the command have.
        command = (char *)malloc(sizeof(char) * 510); // Allocating a char array in size of 510 bytes as demanded..
        if (command == NULL)
        { //if there is an allocation problem exit the program
            printf("ERROR");
            exit(1);
        }
//=================================================================================================================================================//    
        fgets(command, 510, stdin);
        if(got_signal==1) continue;
        if (strcmp(command, "\n") == 0) //If entered an empty prompt line.
        {
            /*free(command);
            free(to_exec);
            free(exec_right);
            free(exec_left);
            free(temp_copy);
            free(safe_copy);
            free(temp_cmd);
            free(argument);
            free(new_exec);*/
            continue;
        }
        commands_length += strlen(command);
        command = (char *)realloc(command, strlen(command) * sizeof(char)); //realloc for command for get off the '\n'  
        command[strlen(command) - 1] = '\0';                                //Manual putting a NULL in the last byte of the array.
        write(log_fd,command,strlen(command));                              // Write the command given by user to Log.txt.
        write(log_fd,"\n",sizeof(char));     
        safe_copy = (char*)malloc(sizeof(char)*strlen(command));      // A safe copy of the original command.
        if (safe_copy == NULL)
        {   //if there is an allocation problem exit the program
            printf("ERROR");
            exit(1);
        }
        temp_copy = (char*)malloc(sizeof(char)*strlen(command));      // A temporary copy to work on of the original command.
        if (temp_copy == NULL)
        {   //if there is an allocation problem exit the program
            printf("ERROR");
            exit(1);
        }
        for(int j = 0; j < strlen(command); j++)
        {
            if(command[j] !=' ' && command[j] != '|' && command[j]!='>') last_letter =j; //This will save the index of the last letter in the cmd for later..
            if (command[j] == '|') // If noticed a pipe:
            {
                command[j] = ' '; // Replace the '|' with space.
          //      free(temp_cmd);
                temp_cmd = (char *)malloc(sizeof(char) * j); //Allocate memory to hold the part of the cmd before the pipe.
                if(j>0&&command[j-1]==' ') 
                    memcpy(temp_cmd,command,last_letter+1); //Copies all of the cmd before the pipe, not including the pipe itself.
                else
                    memcpy(temp_cmd, command, j-1); //Copies all of the cmd before the pipe, not including the pipe itself.
                temp_cmd_size = j;
                to_pipe = 1; //Turn on the pipe flag.
            }
            else if(command[j] == '>') // If noticed a '>', we need to check which direction is requested.
            {
                cutting_index=j; //Save the index of the re-director.
                if(j>0&&j<strlen(command)-1&&command[j+1]=='>'&&command[j-1]!='>') //This checks if the operator is '>>'; if so, replace with spaces and set the direction to - 3.
                {
                    command[j] = ' ';
                    command[j+1] = ' ';
                    direction=3;
                    j++;
                }
                else if(j>0&&j<strlen(command)-1&&command[j+1]!='>'&&command[j-1]=='2') //This checks if the operator is '2>'; if so, replace with spaces and set the direction to - 4.
                {
                    command[j] = ' ';
                    command[j-1] = ' ';
                    direction=4;
                    j++;
                }
                else if(j>0&&j<strlen(command)-1&&command[j+1]!= '>'&& command[j-1]!='>') // Nakes sure if this is '>' operator; if so, replace with a space and set direction to - 1.
                {
                    command[j] = ' ';
                    direction = 1;
                }
                
            }
            else if(command[j] == '<') // If noticed '<', replace with a space and set direction to - 2.
            {
                cutting_index=j;
                command[j] = ' ';
                direction = 2;
            }
        }
        safe_copy = strcpy(safe_copy,command); // A safe copy of the original command.
        temp_copy = strcpy(temp_copy,safe_copy); // A temporary copy to work on of the original command.
        if(direction>0)
        {
            to_direct=1; // Make sure flag is up if noticed a direction operator.
            direction_index=0;
            char *temp = strtok(command, " ");
            while (temp != NULL) // A loop checking how many arguments appear before the direction operator.
            {
                temp = strtok(NULL, " ");
                direction_index++;
            }
            direction_index--;
            free(temp);
        }

        if (to_pipe == 1) // If pipe appeared.
        {
            if(to_direct==1) // If also a director appeared, cut the command without the last argument, which is the file's name.
            {

                memcpy(temp_cmd,temp_cmd,cutting_index);
            }
            pipe_index = 0;
            char *temp = strtok(temp_cmd, " ");
            while (temp != NULL) // A loop to check how many arguments there are before the pipe.
            {
                temp = strtok(NULL, " ");
                pipe_index++;
            }
            free(temp);
        }

        for (int i = 0; i < strlen(command); i++)
        {
            if (command[i] == ' ')
                space_count++;
        }
        space_count += 2;
        to_exec = (char **)malloc(sizeof(char *) * space_count); // Allocating an array for the arguments given in the command according to space_count.
        if (to_exec == NULL)
        { //if there is an allocation problem exit the program
            printf("ERROR");
            exit(1);
        }
        to_exec_size = space_count;
        argument = strtok(temp_copy, " ");
        index = 0;
        while (argument != NULL)
        {
            argument_size = strlen(argument);                              // Save the length of the argument.
            to_exec[index] = (char *)malloc(sizeof(char) * argument_size); // Use the value saved above to allocate memory for the argument in the to_exec array.
            strcpy(to_exec[index], argument);                              // Copy the argument into the to_exec array.
            argument = strtok(NULL, " ");                                  // Copy the next argument in the command.
            index++;
        }
        if (strcmp(to_exec[0], "cd") == 0)
        {
            printf("This command is not supported (yet).\n");

            for (int j = 0; j < to_exec_size-1; j++)
            {
                free(to_exec[j]);
            }      
            free(to_exec);
            free(command);
            free(exec_right);
            free(exec_left);
            free(temp_copy);
            free(safe_copy);
            free(temp_cmd);
            free(argument);
            free(new_exec);
            continue;
        }
        if (strcmp(to_exec[0], "done") != 0 && strcmp(to_exec[0], "cd") != 0)
        {

            int x = 1;
            for(int j = 0; j < index; j++) // A loop to check if there's a need for an update of the pipe index.
            {
                 if (strcmp(to_exec[j], "|") == 0)
                    {
                        to_pipe = 1;
                        pipe_index = j;
                    }
            }
            for (int z = 0; z < index; z++) // A loop to check if there's a need for an update of the direction index
            {
                if (strcmp(to_exec[z], ">") == 0)
                {
                    to_direct = 1;
                    direction = 1;
                    direction_index = z;
                }
            }
            if (to_pipe == 1) // If pipe flag is up.
            {
                commands__pipe++;
                temp_indx = pipe_index;
                exec_left = (char **)malloc(sizeof(char *) * pipe_index); // Allocate memory for the arguments before the pipe operator.
                if (exec_left == NULL)
                {   //if there is an allocation problem exit the program
                    printf("ERROR");
                    exit(1);
                }
                exec_right = (char **)malloc(sizeof(char *) * (index - pipe_index)); // Alocate memory the arguments after the pipe operator.
                if (exec_right == NULL)
                {   //if there is an allocation problem exit the program
                    printf("ERROR");
                    exit(1);
                }
                char* first_arg =(char*) malloc(strlen(temp_copy)*sizeof(char));
                first_arg = strtok(temp_copy," ");
                exec_left[0] = (char*) malloc ( strlen(first_arg));
                strcpy(exec_left[0],first_arg);
                for (int z = 1; z < temp_indx; z++)
                {
                    exec_left[z] = (char *)malloc(sizeof(char) * strlen(to_exec[z]));
                    strcpy(exec_left[z], to_exec[z]); // Copies all arguments left-side of the pipe.
                }
                if(to_direct==1) // If there is a pipe before a redirection.
                {
                    commands_redirection++;
                    for (int z = 0; z < index - pipe_index-1; z++)
                    {
                        exec_right[z] = (char *)malloc(sizeof(char) * strlen(to_exec[temp_indx]));
                        strcpy(exec_right[z], to_exec[temp_indx]); // Copies all argument right-side of the pipe.
                        temp_indx++;
                    }
                }
                else if( to_direct == 0) // If there is only a pipe.
                {
                    for (int z = 0; z < index - pipe_index; z++)
                    {
                        exec_right[z] = (char *)malloc(sizeof(char) * strlen(to_exec[temp_indx]));
                        strcpy(exec_right[z], to_exec[temp_indx]); // Copies all argument right-side of the pipe.
                        temp_indx++;
                    }
                } 
                int left = fork(); // Initiate left son process.
                int right = fork(); // Initiate right son process.
                if (left == 0 && right == 0) // If both sons interfer each-other, exit.
                {
                    exit(0);
                }
                if (left == 0) // Left son, will execute everything before the pipe and redirect the STDOUT to the pipe.
                { 
                    close(pipe_fd[0]);
                    dup2(pipe_fd[1], STDOUT_FILENO);
                    temp_indx = 0;
                    index = 0;
                    execvp(exec_left[0], exec_left);
                    close(pipe_fd[1]);
                    exit(0); 
                }
                if (right == 0 && to_direct == 0) // Right son without a redirection, redirect it's STDIN to be from the pipe and then will execute everything after the pipe on what's given before it. 
                {
                    char* temp = (char*)malloc(sizeof(char));
                    close(pipe_fd[1]);
                    int val = dup2(pipe_fd[0], STDIN_FILENO);
                    if(read(val,temp,sizeof(char))==0)
                    {
                        printf("ERROR: Illegal command before pipe, please try again.\n");
                        exit(0);
                    }
                    execvp(exec_right[0], exec_right);
                    close(pipe_fd[0]);
                    exit(0); 
                }
                else if (right == 0 && direction == 1) // Right son with a redirection, redirects it;s STDIN to be from the pipe, opens the file given by the user and redirects its STDOUT to it and truncates the original content..
                {
                    char* temp = (char*)malloc(sizeof(char));
                    int pfd;
                    close(pipe_fd[1]);
                    dup2(pipe_fd[0], STDIN_FILENO);
                    pfd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                    int val = dup2(pfd, STDOUT_FILENO);
                    if(read(val,temp,sizeof(char))==0)
                    {
                        printf("ERROR: Illegal command before pipe, please try again.\n");
                        continue;
                    }
                    execvp(exec_right[0], exec_right);
                    close(pipe_fd[0]);
                    exit(0); 
                }
                else if (right == 0 && direction == 3) // Right son with a redirection, redirects it;s STDIN to be from the pipe, opens the file given by the user and redirects its STDOUT to it and appends to the original content..
                {
                    char* temp = (char*)malloc(sizeof(char));
                    int pfd;
                    close(pipe_fd[1]);
                    dup2(pipe_fd[0], STDIN_FILENO);
                    pfd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
                    int val = dup2(pfd, STDOUT_FILENO);
                    if(read(val,temp,sizeof(char))==0)
                    {
                        printf("ERROR: Illegal command before pipe, please try again.\n");
                        exit(0);
                    }
                    execvp(exec_right[0], exec_right);
                    close(pipe_fd[0]);
                    exit(0); // If the command is unknown.
                }
                else if (right == 0 && direction == 4) // Right son with a redirection, redirects it;s STDIN to be from the pipe, opens the file given by the user and redirects its STDOUT to STDERR.         
                {
                    char* temp = (char*)malloc(sizeof(char));
                    int pfd;
                    close(pipe_fd[1]);
                    int val = dup2(pipe_fd[0], STDIN_FILENO);
                    if(read(val,temp,sizeof(char))==0)
                    {
                        printf("ERROR: Illegal command before pipe, please try again.\n");
                        exit(0);
                    }
                    pfd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                    dup2(pfd, STDERR_FILENO);
                    execvp(exec_right[0], exec_right);
                    close(pipe_fd[0]);
                    exit(0); 
                }
                if (getpid() != 0) // Father will wait until son is done.
                {
                    close(pipe_fd[1]);
                    close(pipe_fd[0]);
                    wait(NULL); 
                    wait(NULL);
                }
            }
            if (to_direct == 1 && to_pipe == 0) // If there's only a redirection.
            {
                commands_redirection++;
                if (direction == 1) // '>' Operator, direct the output to a given file and truncate it's content.
                {
                    int pid = fork();
                    if (pid == 0)
                    {
                            fd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                            int val = dup2(fd, STDOUT_FILENO);
                            //free(new_exec);
                            new_exec = (char **)malloc(sizeof(char *) * direction_index);
                            if (new_exec == NULL)
                            { //if there is an allocation problem exit the program
                                printf("ERROR");
                                exit(1);
                            }
                            for (int i = 0; i < direction_index; i++)
                            {
                                new_exec[i] = (char *)malloc(sizeof(char) * strlen(to_exec[i]));
                                new_exec[i] = strcpy(new_exec[i], to_exec[i]);
                            }
                            execvp(new_exec[0], new_exec);
                            exit(0);
                    }
                    if(getpid()!=0) 
                        wait(NULL);
                }
                else if(direction == 2) // '<' Operator, directs a given file to be an input for a program/func.
                {
                    int pid = fork();
                    if (pid == 0)
                    {
                            fd = open(to_exec[direction_index], O_RDONLY | O_CREAT ,  S_IRWXU);
                            int val = dup2(fd, STDIN_FILENO);
                           // free(new_exec);
                            new_exec = (char **)malloc(sizeof(char *) * direction_index);
                            if (new_exec == NULL)
                            { //if there is an allocation problem exit the program
                                printf("ERROR");
                                exit(1);
                            }
                            for (int i = 0; i < direction_index; i++)
                            {
                                new_exec[i] = (char *)malloc(sizeof(char) * strlen(to_exec[i]));
                                new_exec[i] = strcpy(new_exec[i], to_exec[i]);
                            }
                            execvp(new_exec[0], new_exec);
                            exit(0);
                    }
                    if(getpid()!=0) 
                        wait(NULL);
                }
                else if(direction==3) // '>>' Operator, append the output to a given file 
                {
                    int pid = fork();
                    if (pid == 0)
                    {
                            fd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
                            int val = dup2(fd, STDOUT_FILENO);
                          //  free(new_exec);
                            new_exec = (char **)malloc(sizeof(char *) * direction_index);
                            if (new_exec == NULL)
                            { //if there is an allocation problem exit the program
                                printf("ERROR");
                                exit(1);
                            }
                            for (int i = 0; i < direction_index; i++)
                            {
                                new_exec[i] = (char *)malloc(sizeof(char) * strlen(to_exec[i]));
                                new_exec[i] = strcpy(new_exec[i], to_exec[i]);
                            }
                            execvp(new_exec[0], new_exec);
                            exit(0);
                    }
                    if(getpid()!=0) 
                        wait(NULL);
                    
                }
            }
             if (direction == 4) // '2>' Operator, direct the output of something to STDERR.
                {
                    int pid = fork();
                    if (pid == 0)
                    {
                            fd = open(to_exec[direction_index], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                            int val = dup2(fd, STDERR_FILENO);
                            new_exec = (char **)malloc(sizeof(char *) * direction_index);
                            if (new_exec == NULL)
                            { //if there is an allocation problem exit the program
                                printf("ERROR");
                                exit(1);
                            }
                            for (int i = 0; i < direction_index; i++)
                            {
                                new_exec[i] = (char *)malloc(sizeof(char) * strlen(to_exec[i]));
                                new_exec[i] = strcpy(new_exec[i], to_exec[i]);
                            }
                            execvp(new_exec[0], new_exec);
                            exit(0);
                    }
                    if(getpid()!=0) 
                        wait(NULL);
                }
            else if(to_direct==0 && to_pipe == 0) // If there's no pipe operator, nor redirection operator.
            {
                x = fork();            // Initiate a new son process.
                childid=x;
                if (getpid() != 0)
                {
                    wait(NULL); // Father will wait until son is done.
                }
                if (x == 0)
                {
                    signal(SIGTSTP, sigtstpHandler);
                    execvp(to_exec[0], to_exec); // Son's job, executing the user's command.
                    exit(0); 
                }
            }
        }
        if (strcmp(to_exec[0], "done") == 0)
        {
            running = 0; //Stop the loop.
            double average = (double)commands_length / (double)command_count;
            printf("Number of commands executed: %d \n", command_count);
            printf("Total length of all the commands: %d \n", commands_length);
            printf("Average length of a command: %f \n", average);
            printf("Number of commands include pipe operator: %d \n",commands__pipe);
            printf("Number of commands that includes redirection: %d \n",commands_redirection);
            printf("See you next time! \n");
            if (to_direct == 1 || to_pipe == 0)
            {
                for (int j = 0; j < to_exec_size-1; j++)
                {
                    free(to_exec[j]);
                }
                free(to_exec);
            }
            exit(0);
        }
    
    //#-------------------FREES----------------------#//
           for (int j = 0; j < to_exec_size-1; j++)
            {
                free(to_exec[j]);
            }
            free(to_exec);
            free(command);
    }
    return 0;
}
