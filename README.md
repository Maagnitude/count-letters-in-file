# count-letters-in-file

A C program that manages read/write access to a file using semaphores, for an assignment in Operating Systems.
It forks the process and then the parent process has to write 2000 random a-z chars in **data.txt**, and the child process has
to read the file and count the occurrences of each letter. Then, the child process, prints them on screen, and the parent
process terminates the program.
