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
sem_t semaphore;//создаем семафор для нескольких потоков

void sigalrm_handler(int signum) { // обработаем сигнал работы демона
	flag = 1;
	return;
}

void sigterm_handler(int signum) { // обработаем сигнал завершения программы
	flagDie = 1;
	return;
}

void sigchild_handler(int signum) {// для дочернего процесса
	flagWait = 1;
	return;
}

int Daemon(char* filename) {
	signal(SIGALRM, sigalrm_handler);// сигнал начала работы
	signal(SIGTERM, sigterm_handler);// сигнал прерывания
	signal(SIGCHLD, sigchild_handler);// сигнал разблокировки семафора после дочернего процесса

	sem_init(&semaphore, 0, 1); // инициализируем семафор

	while (1) {
		pause(); //останавливаем процесс, ожидаем сигнал
		if (flag == 1) { // выполняем, когда сигнал пойман
			// считаем команды из файла, разобьем их, и поместим в массив для дальнейшей обработки
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
			for (int i = 0; i < commands_iter; i++) { // обработка комманд
				pid_t ppid;
				if ((ppid = fork()) == 0) { // дочерний процесс исполнит команду и завершится

					int cnt_str = 0;
					char* pch = strtok(commands[i], " ");
					char* command[100];
					while (pch != NULL)
					{
						command[cnt_str++] = pch;
						pch = strtok(NULL, " ");
					}
					command[cnt_str] = NULL;
					int wait = sem_wait(&semaphore);//блокируем семафор
					if (wait == -1) {// если блокировка произошла с ошибкой
						char error[100];// массив с ошибками
						sprintf(error, "%s%d%s", "Команда ", i + 1, " не может выполниться\n");
					}
					else {//ошибок не было

						char log_message[100];
						sprintf(log_message, "%s%d%s", "Выполнена команда ", i + 1, "\n");//сохраним выполненную команду, потом добавим в файл

						int logFile = open("log.txt", O_CREAT | O_RDWR, S_IRWXU); // файл, с выполненными командами
						lseek(logFile, 0, SEEK_END);//сдвиг в конец файла
						write(logFile, log_message, strlen(log_message));//пишем команду
						close(logFile);

						close(1);
						int fileOut = open("output.txt", O_CREAT | O_RDWR | O_APPEND, S_IRWXU);
						dup2(fileOut, 1);
						execv(command[0], command);
					}


				}
				else { //родительский процесс
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
		if (flagDie == 1) { // если поймали сигнал завершения процесса
			sem_destroy(&semaphore);// уничтожаем семафор
			printf("Демон уничтожен");
			exit(0);
		}
	}

}

int main(int argc, char* argv[])
{
	pid_t parpid;
	if ((parpid = fork()) < 0) // не получается создать дочерний процесс
	{
		printf("\ncan't fork");
		exit(1); // ошибка
	}
	else if (parpid != 0) { exit(0); } // если родитель, то выходим
	char* filename = argv[1];
	setsid();  // переводим наш дочерний процесс в новую сесию
	Daemon(filename);  // вызов демона
	return 0;
}