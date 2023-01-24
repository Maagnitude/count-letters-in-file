#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>       	// for O_RDONLY, O_WRONLY etc. OFLAGS' definitions.
#include <sys/stat.h>    	// for modes, permissions etc.
#include <sys/wait.h>
#include <semaphore.h>

		// Link with -pthread.

#define NUM_THREADS 4		// Number of threads.
#define NUM_CHARS 2000		// Number of characters for the file.

int count_all_letters = 0;	// To store all the letter counted by child.

int count_thread[NUM_THREADS];	// A table that stores each thread.

// A mutex to synchronize access to the shared count table
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

// Calculating the chunk size as the division of the number of chars, by the number of threads
int chunk_size = NUM_CHARS / NUM_THREADS;

// A table with size=26 to store the count of each character (A-Z)
int count[26] = {0};

void* thread_function(void* arg);	// thread fuction signature

void sigint_handler(int sig);		// signal handler signature


int main(int argc, char* argv[]) {

    const char *semName = "it214124";	// Initialize semaphore's name
    sem_t *sem_filelock;		// declaring the semaphore
    sem_filelock = sem_open(semName, O_CREAT, 0600, 0);		// We give 0 so that parent starts 
								// always first, the writing part.
    if (sem_filelock == SEM_FAILED) {
        perror("sem_open failed");
        exit(1);
    }
    
    pid_t child_pid = fork();		// process fork

    if (child_pid == -1) {
        perror("Fork failed");
        exit(1);
    }

    if (child_pid == 0) {
    // This is the child process (the reader process)

        signal(SIGINT, sigint_handler);		// for signal handler
        signal(SIGTERM, sigint_handler);	// for signal handler (both okay)

	if (sem_wait(sem_filelock) == -1) {
            perror("sem_wait failed");
            exit(1);
        }
	    
	printf("Child: Started.\n");
	
	sleep(2);

	// Create an array of thread IDs
	pthread_t threads[NUM_THREADS];

	printf("Child: Creats 4 threads.\n");
	// Create the threads
	for (int i = 0; i < NUM_THREADS; i++) {
	    count_thread[i] = i;	
	    int ret = pthread_create(&threads[i], NULL, thread_function, &count_thread[i]);
	    if (ret != 0) {
                perror("pthread_create failed");
                exit(1);
            }
	}

	// Wait for the threads to finish
	for (int i = 0; i < NUM_THREADS; i++) {
	    int ret = pthread_join(threads[i], NULL);
	    if (ret != 0) {
                perror("pthread_join failed");
                exit(1);
            }
	}

	printf("Child: Prints counter for each character.\n");
	// Print the count of each character
	for (int i = 0; i < 26; i++) {
	    printf("%c: %d\n", 'a' + i, count[i]);
	    usleep(200000);
	}

	sleep(2);

        for (int i = 0; i < 26; i++) {
            count_all_letters += count[i];   
        }

        printf("Child: Total number of letters in file: %d.\n", count_all_letters);
    
    } else {
    // This is the parent process (the writer process)

        signal(SIGINT, SIG_IGN);	// for the parent process
        signal(SIGTERM, SIG_IGN);	// to ignore the signal
	
	printf("Parent: Started.\n");
	// Open the file for writing
	int fd = open("data.txt", O_CREAT | O_WRONLY, 00777);

	if (fd == -1) {
            perror("Opening file descriptor failed");
            exit(1);
        }

	printf("Parent: Opened file.\n");

	// Generate a string of random characters
	char buf[NUM_CHARS + 1];
	int urandom = open("/dev/urandom", O_RDONLY);

	if (urandom == -1) {
            perror("Opening /dev/urandom failed");
            exit(1);
        }
        
	if (read(urandom, buf, NUM_CHARS) == -1) {
            perror("Read from /dev/urandom failed");
            exit(1);
        }
	close(urandom);
	int counterletter = 0;
	for (int i = 0; i < NUM_CHARS; i++) {
	    buf[i] = 'a' + ((int)buf[i] % 26);
	    if (buf[i] > 96 && buf[i] < 123) {
	        counterletter++;
	    } else {
	        i--;
	    }
	}
	buf[NUM_CHARS] = '\0';

	// Write the string to the file
	if (write(fd, buf, NUM_CHARS) == -1) {
            perror("Write to file failed");
            exit(1);
        }

	printf("Parent: Letters writen in file, and counted: %d.\n", counterletter);
	
	sleep(2);

	// Close the file
	close(fd);

	if (sem_post(sem_filelock) == -1) {
            perror("sem_post failed");
            exit(1);
        }

	printf("Parent: Waits for the child.\n");

	// Wait for the child process to finish using waitpid()
	waitpid(child_pid, NULL, 0);

	printf("Parent: The child got a signal, and I terminated normally\n");

	sem_close(sem_filelock);
	if (sem_unlink(semName) == -1) {
            perror("sem_unlink failed");
            exit(1);
        }
    }  
    
    return 0;  
}

void* thread_function(void* arg) {		

    // Open the file for reading
    int fd = open("data.txt", O_RDONLY, 00777);

    if (fd == -1) {
        perror("Open file failed in thread");
        pthread_exit(NULL);
    }

    // Seek to the correct position in the file
    lseek(fd, (long)arg * chunk_size, SEEK_CUR);

    // Read the chunk of data from the file
    char buf[chunk_size + 1];

    size_t num_read = read(fd, buf, chunk_size);
    if (num_read == -1) {
        perror("Read from file failed in thread");
        close(fd);
        pthread_exit(NULL);
    }
    buf[num_read] = '\0';

    // Close the file
    close(fd);

    int partialcount[26] = {0};
    //Process the chunk of data and update the count table
    for (int i = 0; i < num_read; i++) {
       int index = buf[i] - 'a';
       if (index >= 0 && index < 26) {
           partialcount[index] ++;
       }
    }

    pthread_mutex_lock(&count_mutex);
    for (int i = 0; i < 26; i++) {
        count[i] += partialcount[i];
    }
    pthread_mutex_unlock(&count_mutex); 

    return NULL;
}

// Signal handler to terminate the child process
void sigint_handler(int sig) {
    char c;

    printf("\nChild: Do you really want to terminate the program? (y/n): ");
    fflush(stdout);

    // Wait for the user to enter a character
    scanf("%c", &c);

    if (c == 'y' || c == 'Y') {
        //Terminate the program
	exit(0);
    } else {
        printf("Child: Continuing the counting...");
    }
}

