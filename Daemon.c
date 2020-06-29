#include <stdio.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <resolv.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>

int flag = 0;
int flagDie = 0;
int flagWait = 0;
sem_t semaphore;//create a semaphore for threads

void sigalrm_handler(int signum) { // process the signal
	flag = 1;
	return;
}

void sigterm_handler(int signum) { // process the signal to end the program
	flagDie = 1;
	return;
}

void sigchild_handler(int signum) {// for child process
	flagWait = 1;
	return;
}

int Daemon(char* filename) {
	signal(SIGALRM, sigalrm_handler);// start signal
	signal(SIGTERM, sigterm_handler);// interrupt signal
	signal(SIGCHLD, sigchild_handler);// semaphore unlock signal after child process

	sem_init(&semaphore, 0, 1); // initialize the semaphore

	while (1) {
		pause(); //stop the process, wait for a signal
		if (flag == 1) { // execute when the signal is caught
			//we read the commands from the file, break them, and put them in an array for further processing
			char buf[1000] = "";
			int fd = open(filename, O_CREAT | O_RDWR, S_IRWXU);
			read(fd, buf, sizeof(buf));
			close(fd);
			flag = 0;

			char* commands[1000];
			int commands_iter = 0;

			char* pch = strtok(buf, "\n");
			while (pch != NULL) {
				commands[commands_iter] = pch;
				commands[commands_iter++][strlen(pch)] = '\0';
				pch = strtok(NULL, "\n");
			}

			commands[commands_iter] = NULL;
			for (int i = 0; i < commands_iter; i++) { // command processing
				pid_t ppid;
				if ((ppid = fork()) == 0) { // the child process will execute the command and end

					int cnt_str = 0;
					char* pch = strtok(commands[i], " ");
					char* command[100];
					while (pch != NULL)
					{
						command[cnt_str++] = pch;
						pch = strtok(NULL, " ");
					}
					command[cnt_str] = NULL;
					int wait = sem_wait(&semaphore);// block the semaphore
					if (wait == -1) {// if the lock occurred with an error
						char error[100];// array with errors
						sprintf(error, "%s%d%s", "Command ", i + 1, " cannot execute\n");
					}
					else {//ошибок не было

						char log_message[100];
						sprintf(log_message, "%s%d%s", "Command completed ", i + 1, "\n");//save the executed command, then add to the file

						int logFile = open("log.txt", O_CREAT | O_RDWR, S_IRWXU); // file with executed commands
						lseek(logFile, 0, SEEK_END);// shift to end of file
						write(logFile, log_message, strlen(log_message));// write command
						close(logFile);

						close(1);
						int fileOut = open("output.txt", O_CREAT | O_RDWR | O_APPEND, S_IRWXU);
						dup2(fileOut, 1);
						execv(command[0], command);
					}


				}
				else { // parent process
					while (1) {
						pause();
						if (flagWait == 1) {
							wait(NULL);
							sem_post(&semaphore);
							flagWait = 0;
							break;
						}
					}
				}

			}
		}
		if (flagDie == 1) { // if caught the signal of completion of the process
			sem_destroy(&semaphore);// destroy the semaphore
			printf("Daemon killed");
			exit(0);
		}
	}

}

int main(int argc, char* argv[])
{
	pid_t parpid;
	if ((parpid = fork()) < 0) // cannot create child process
	{
		printf("\ncan't fork");
		exit(1); // error
	}
	else if (parpid != 0) { exit(0); } // if parent, then exit
	char* filename = argv[1];
	setsid();  // we transfer our child process to a new session
	Daemon(filename);  // demon call
	return 0;
}