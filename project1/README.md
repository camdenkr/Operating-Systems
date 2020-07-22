By: Camden Kronhaus

A simple shell created using C. 

When the program first runs, it executes the prompt (if no "-n" arguemnt is passed) and enters an infinite REPL loop.

When command is input, the command line is taken as an inpiut. The line is parsed for metachracters and commands and redirected to a 2D array of the commands and metacharacters. CTRL-D will exit the shell program. parseLine parses using a method of each line in the 2D array ending in a metachracter, if there is one. Separating consistently made executing the commands much easier.

If a "<" the file after the metachraracter is fed as the input into the leading command.
If a ">" is detected command output is redirected to the following file.
If "|" is detected the program will pipe accordingly.
If "&" is detected the program will run processes inthe background.

The array is then redirected to be executed in execLine(). It reads line by line, using the last string in each row to find it's metachracter until all the commands are completed, prompt()is then executed form main again.

Problems:
    Currently certain combintations of pipes do not function, pipes will sometimes read an empty pipe and output accordingly, when it should be reading from the previous pipe. The input of the previous pipe needs to be read into the current pipe and more work needs to done to fix this.