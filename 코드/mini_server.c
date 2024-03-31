#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client {
    int id;
    size_t msg_size;
    char *msg;              // msg를 담고 있는
}	t_client;


fd_set readfds, writefds, active;
t_client clients[1024]; // 255 -> 1024
int fd_max = 0, id_next = 0;
char buffer_read[120000], buffer_write[120000];
char *large_buffer;

void ft_error(const char *str) {
    if (str) {
        write(2, str, strlen(str));
    } else {
        write(2, "Fatal error", strlen("Fatal error"));
    }
    write(2, "\n", 1);
    exit(1);
}

int count_number(int num) {
    if (num == 0) {
        return 1;
    }
    int count = 0;
    while (num) {
        num /= 10;
        ++count;
    }
    return count;
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void send_all(int except_fd) {
	for (int cur_fd = 0; cur_fd <= fd_max; ++cur_fd) {
		if (FD_ISSET(cur_fd, &writefds) && cur_fd != except_fd) {
            if (large_buffer) {
                send(cur_fd, large_buffer, strlen(large_buffer), 0);
            } else {
                send(cur_fd, buffer_write, strlen(buffer_write), 0);
            }
		}
	}
}

void init_server(int *sockfd, const char *port) {
	struct sockaddr_in servaddr;

	*sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (*sockfd == -1) { 
        ft_error(NULL);
	} 
	//printf("Socket successfully created..\n"); 


	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(port));

	if ((bind(*sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) { 
		ft_error(NULL);
	}
	//printf("socket bind successfully...\n"); 

	if (listen(*sockfd, 10) < 0) {
		ft_error(NULL);
	}
	//printf("socket listen successfully...\n"); 
}

void accept_new_client(int sockfd) {
	struct sockaddr_in cli; 
	socklen_t len;
	int connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0) {
		return ;
	}
	//printf("server acccept success...\n"); 

	fd_max = connfd > fd_max ? connfd : fd_max;
	//printf("fd_max: %d\n", fd_max);
	clients[connfd].id = id_next++;
    clients[connfd].msg_size = 1024;
    clients[connfd].msg = (char *)malloc(sizeof(char) * clients[connfd].msg_size);
    if (!clients[connfd].msg) {
        ft_error(NULL);
    }
	FD_SET(connfd, &active);
	sprintf(buffer_write, "server: client %d just arrived\n", clients[connfd].id);
    printf("server: client %d just arrived\n", clients[connfd].id);
	//printf("%s", buffer_write);
	send_all(connfd);
}

void handle_client_message(int cur_fd) {
    int msg_len = 0;
	ssize_t res = recv(cur_fd, buffer_read, 65536, 0);
	if (res <= 0) {
		sprintf(buffer_write, "server: client %d just left\n", clients[cur_fd].id);
		//printf("%s\n", buffer_write);
		send_all(cur_fd);
		FD_CLR(cur_fd, &active);	// order 변경
		close(cur_fd);
        free(clients[cur_fd].msg); clients[cur_fd].msg = NULL;
		return ;					// return 빼먹음
	}
	for (size_t i = 0, j = strlen(clients[cur_fd].msg); i < (size_t)res; ++i, ++j) { // buffer_read 아님
        if (j >= clients[cur_fd].msg_size) {
            free(clients[cur_fd].msg); clients[cur_fd].msg = NULL;
            clients[cur_fd].msg_size += 1024;
            clients[cur_fd].msg = (char *)calloc(clients[cur_fd].msg_size, sizeof(char));
            if (!clients[cur_fd].msg) {
                ft_error(NULL);
            }
        }
		clients[cur_fd].msg[j] = buffer_read[i];         // 1024넘어가면 어떻게하려고...
		if (clients[cur_fd].msg[j] == '\n') {
			clients[cur_fd].msg[j] = '\0';
            msg_len = strlen("client : \n") + count_number(clients[cur_fd].id) + strlen(clients[cur_fd].msg);
            printf("%s\n", clients[cur_fd].msg);
            // printf("msg_len: %lu + %d + %lu = %d\n", strlen("client : \n"), count_number(clients[cur_fd].id), strlen(clients[cur_fd].msg), msg_len);
            if (msg_len > 120000) {
                large_buffer = (char *)calloc(sizeof(char) * msg_len, sizeof(char));
                if (!large_buffer) {
                    ft_error(NULL);
                }
                sprintf(large_buffer, "client %d: %s\n", clients[cur_fd].id, clients[cur_fd].msg);
            } else if (msg_len > 0 && msg_len <= 120000) {
                sprintf(buffer_write, "client %d: %s\n", clients[cur_fd].id, clients[cur_fd].msg);
            } 			
            send_all(cur_fd);
            free(large_buffer); large_buffer = NULL;
			j = -1;
            clients[cur_fd].msg_size = 1024;
            free(clients[cur_fd].msg); clients[cur_fd].msg = NULL;
            clients[cur_fd].msg = (char *)calloc(clients[cur_fd].msg_size, sizeof(char));
            if (!clients[cur_fd].msg) {
                ft_error(NULL);
            }
		}
	}
}

int main(int argc, char *argv[]) {
    int sockfd;

    if (argc != 2) {
        ft_error("Wrong number of arguments");
    }

    init_server(&sockfd, argv[1]);

	FD_ZERO(&active);
	FD_SET(sockfd, &active);						// ㅋㅋ ISSET뭐임
	fd_max = sockfd;								// 위치가 바뀜
	//printf("FD setting successfully...\n");
	bzero(clients, sizeof(clients));

	while (1) { 									// while... 왜 안씀ㅋㅋ
		readfds = writefds = active;
		if (select(fd_max + 1, &readfds, &writefds, NULL, NULL) < 0) {
			ft_error(NULL);
		}
		for (int cur_fd = 0; cur_fd <= fd_max; ++cur_fd) {

			if (FD_ISSET(cur_fd, &readfds)) {
				if (sockfd == cur_fd) {
					accept_new_client(sockfd);
				}
				else {
					handle_client_message(cur_fd);
				}
			}
		}
	}
    return 0;
}

