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
sem_t semaphore;//������� ������� ��� ���������� �������

void sigalrm_handler(int signum) { // ���������� ������ ������ ������
	flag = 1;
	return;
}

void sigterm_handler(int signum) { // ���������� ������ ���������� ���������
	flagDie = 1;
	return;
}

void sigchild_handler(int signum) {// ��� ��������� ��������
	flagWait = 1;
	return;
}

int Daemon(char* filename) {
	signal(SIGALRM, sigalrm_handler);// ������ ������ ������
	signal(SIGTERM, sigterm_handler);// ������ ����������
	signal(SIGCHLD, sigchild_handler);// ������ ������������� �������� ����� ��������� ��������

	sem_init(&semaphore, 0, 1); // �������������� �������

	while (1) {
		pause(); //������������� �������, ������� ������
		if (flag == 1) { // ���������, ����� ������ ������
			// ������� ������� �� �����, �������� ��, � �������� � ������ ��� ���������� ���������
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
			for (int i = 0; i < commands_iter; i++) { // ��������� �������
				pid_t ppid;
				if ((ppid = fork()) == 0) { // �������� ������� �������� ������� � ����������

					int cnt_str = 0;
					char* pch = strtok(commands[i], " ");
					char* command[100];
					while (pch != NULL)
					{
						command[cnt_str++] = pch;
						pch = strtok(NULL, " ");
					}
					command[cnt_str] = NULL;
					int wait = sem_wait(&semaphore);//��������� �������
					if (wait == -1) {// ���� ���������� ��������� � �������
						char error[100];// ������ � ��������
						sprintf(error, "%s%d%s", "������� ", i + 1, " �� ����� �����������\n");
					}
					else {//������ �� ����

						char log_message[100];
						sprintf(log_message, "%s%d%s", "��������� ������� ", i + 1, "\n");//�������� ����������� �������, ����� ������� � ����

						int logFile = open("log.txt", O_CREAT | O_RDWR, S_IRWXU); // ����, � ������������ ���������
						lseek(logFile, 0, SEEK_END);//����� � ����� �����
						write(logFile, log_message, strlen(log_message));//����� �������
						close(logFile);

						close(1);
						int fileOut = open("output.txt", O_CREAT | O_RDWR | O_APPEND, S_IRWXU);
						dup2(fileOut, 1);
						execv(command[0], command);
					}


				}
				else { //������������ �������
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
		if (flagDie == 1) { // ���� ������� ������ ���������� ��������
			sem_destroy(&semaphore);// ���������� �������
			printf("����� ���������");
			exit(0);
		}
	}

}

int main(int argc, char* argv[])
{
	pid_t parpid;
	if ((parpid = fork()) < 0) // �� ���������� ������� �������� �������
	{
		printf("\ncan't fork");
		exit(1); // ������
	}
	else if (parpid != 0) { exit(0); } // ���� ��������, �� �������
	char* filename = argv[1];
	setsid();  // ��������� ��� �������� ������� � ����� �����
	Daemon(filename);  // ����� ������
	return 0;
}