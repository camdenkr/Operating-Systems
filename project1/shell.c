#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>

#define BUFFER 512 //Input cannot exceed 512 bytes

void prompt()
{
    printf("my_shell$ "); //prompt
}

void execLine(char *argv[BUFFER][BUFFER])
{

    char *nullString = NULL;
    char *outputName;
    char *inputName;
    int in;
    int out;
    int savedStdOut;
    int savedStdIn;

    pid_t pid;
    int wstatus;

    int k = 0;
    int l = 0;
    int pipeNum = 0;
    bool ampFlag = 0;

    int inputLine = 999;
    int outputLine = 999;

    //loop through to dup based on if there is an input or output statement
    while (argv[k][l] != NULL) //loop through rows
    {
        while (argv[k][l] != NULL) //loop through columns
        {
            if (!strcmp(argv[k][l], ">"))
            {
                outputName = argv[k + 1][0];
                outputLine = k;
                out = open(outputName, O_RDWR); // writing fd of output.txt to out
                //argv[k][l] = nullString;
                argv[k][l] = nullString;
            }
            else if (!strcmp(argv[k][l], "<"))
            {

                inputName = argv[k + 1][0];
                inputLine = k;
                in = open(inputName, O_RDWR); // writing fd of output.txt to out
                argv[k][l] = nullString;
            }
            else if (!strcmp(argv[k][l], "|"))
            {
                argv[k][l] = nullString;
                pipeNum++;
            }
            else if (!strcmp(argv[k][l], "&"))
            {
                ampFlag = 1;
                argv[k][l] = nullString;
            }
            l++;
        }
        l = 0;
        k++;
    }
    //int numLines = k;

    //create am array of all the pipes
    int *fd[pipeNum];
    int arr[2]; //will hold read and writes of pipes
    int i = 0;

    for (i = 0; i < pipeNum; i++)
    { //create an array of integer arrays for filedescriptors
        //fd[o] = malloc(32*sizeof(int));
        fd[i] = arr;
        //create a pipe for each pipNum
        if (pipe(fd[i]))
        {
            perror("\nERROR: ");
        }
    }

    l = 0;
    k = 0;

    savedStdOut = dup(1); // saved stdout fd
    savedStdIn = dup(0);  // saved stdin fd

    int pipesPassed = 0;
    int runTimes = pipeNum + 1;
    i = 0;
    int j = 0;

    for (i = 0; i < runTimes; i++)
    {
        pid = fork();
        if (pid == 0)
        {
            if (k == outputLine)
            {
                dup2(out, 1); // replace standard output with output file //out is stdout //1 is now the out
                close(out);
            }
            else if (k == inputLine && (k + 1) == outputLine)
            {
                dup2(out, 1); // replace standard output with output file //out is stdout //1 is now the out
                close(out);
                dup2(in, 0); // replace standard output with output file //out is stdout //1 is now the out
                close(in);
            }
            else if (k == inputLine)
            {
                dup2(in, 0); // replace standard output with output file //out is stdout //1 is now the out
                close(in);
            }
            else if (pipeNum > 0)
            {
                if (pipeNum == 1)
                {
                    if (pipesPassed == 0)
                    {
                        //close(fd[0][0]);
                        dup2(fd[0][1], 1);
                        //close(fd[0][1]);
                    }
                    else
                    {
                        //last pipe
                        //close(fd[pipesPassed - 1][1]);
                        dup2(fd[pipesPassed - 1][0], 0);
                        //close(fd[pipesPassed - 1][0]);
                    }
                }
                else
                {
                    if (pipesPassed == 0) // first pipe
                    {
                        //close(fd[0][0]);
                        dup2(fd[0][1], 1);
                        //close(fd[0][1]);
                    }
                    else if (pipesPassed == pipeNum) //AFTER last pipe
                    {                                //last pipe
                        //close(fd[pipesPassed - 1][1]);
                        dup2(fd[pipesPassed - 1][0], 0);
                        //close(fd[pipesPassed - 1][0]);
                    }
                    else //all pipes in the middle
                    {    //(f > 0) and not last one
                        dup2(fd[pipesPassed - 1][0], 0);
                        //close(fd[pipesPassed - 1][0]);
                        dup2(fd[pipesPassed - 1][1], 1);
                        //close(fd[pipesPassed - 1][1]);
                    }
                }
            }

            //!
            // for (s = 0; s < pipeNum; s++) //s=j
            // {
            //     close(fd[s][0]);
            //     close(fd[s][1]);
            // }

            if (execvp(argv[k][0], argv[k]) == -1)
            {
                perror("\nERROR: ");
            }
            dup2(savedStdIn, 0);  //savedstd out becomes 1
            dup2(savedStdOut, 1); //savedstdOut becomes 1
            exit(42);
        }
        else if (pid > 0)
        {
            for (j = 0; j < pipeNum; j++)
            {
                //close(fd[s][0]);
                close(fd[j][1]);
            }
            // dup2(fd[pipesPassed-1][0],0);
            // close(fd[pipesPassed-1][0]);
            if (!ampFlag)
            {
                pid = waitpid(pid, &wstatus, 0);
            }
        }
        else
        {
            //error
            perror("\nERROR: ");
        }

        if (k != outputLine && k != inputLine)
        {
            pipesPassed++;
        }
        k++;
    }
    //!
    // for (s = 0; s < pipeNum; s++)
    // {
    //     close(fd[s][0]);
    //     close(fd[s][1]);
    // }
}

void parseLine(char *cmdLine)
{
    //creating a 2d array of commands, each row for corresponding commands and outputs
    char *execReadyCommands[BUFFER][BUFFER] = {{NULL}};
    //k is rows of execReadyCommands, l is columns of execReadyCommands
    int k = 0;
    int l = 0;

    char *cmdLineSpaceless[BUFFER]; //holds shell commands without whitespace or newline character

    int i = 0;

    char *token; //will hold the tokens grabbed from parsing

    //first get rid of all whitespace to simplify operations
    token = strtok(cmdLine, "  \n");
    cmdLineSpaceless[0] = token;
    //pass in the first command
    /* walk through other tokens */
    while (token != NULL)
    {
        i++;
        token = strtok(NULL, "  \n");
        cmdLineSpaceless[i] = token;
    }
    i = 0;
    //now iterate through inputs previously separated by whitespace, keeping track of if it is a special character
    //special characters separate line by line to make execLine consistent
    while (cmdLineSpaceless[i] != NULL)
    {
        if (!strcmp(cmdLineSpaceless[i], ">") ||
            !strcmp(cmdLineSpaceless[i], "<") ||
            !strcmp(cmdLineSpaceless[i], "&") ||
            !strcmp(cmdLineSpaceless[i], "|"))
        {
            execReadyCommands[k][l] = malloc(20);
            strcpy(execReadyCommands[k][l], cmdLineSpaceless[i]);
            k++;
            l = 0;
        }
        else
        {
            execReadyCommands[k][l] = malloc(20);
            strcpy(execReadyCommands[k][l], cmdLineSpaceless[i]);
            l++;
        }

        i++;
    }
    execLine(execReadyCommands);
}

void readShell(void)
{
    char cmdLine[BUFFER];
    char *line = fgets(cmdLine, BUFFER, stdin);
    if (line == NULL) //CTRL-D
    {
        exit(42);
    }
    parseLine(cmdLine); //other commands parsed
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        while (1) //REPL
        {
            prompt();
            readShell();
        }
    }
    else if (!strcmp(argv[1], "-n")) //don't print prompt
    {
        while (1)
        {
            readShell();
        }
    }
    return 0;
}