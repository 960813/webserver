#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

void log_debug(const char*format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

static const int backlog = 32;

static const size_t initial_buf_capacity = 2048;
static const size_t recv_buf_capacity = 2048;

static bool server_stopped = false;

struct socket_handler_data {
	int fd;
};

static void *accepted_socket_handler(void *arg)
{
	struct socket_handler_data data = *((struct socket_handler_data *)arg);
	free(arg);

	int fd = data.fd;

	log_debug("[%d] thread started: fn:accepted_socket_handler\n", fd);

	char recv_buf[recv_buf_capacity];

	int line_buf_capacity = initial_buf_capacity;
	char *line_buf = malloc(line_buf_capacity);
	int line_buf_len = 0;

	while (1){
		int recv_len = recv(fd, recv_buf, recv_buf_capacity, 0);

		if (recv_len == -1) {
			perror("recv");
			exit(100);
		}

		int line_feed_index = -1;
		for(int i = 0; i < recv_len; ++i) {
			if (recv_buf[i] == '\n') {
				line_feed_index = i;
				break;
			}
		}

		if (line_feed_index != -1) {
			while (line_buf_len + recv_len > line_buf_capacity) {
				line_buf_capacity *= 2;
				line_buf = realloc(line_buf, line_buf_capacity);
			}
			memcpy(line_buf + line_buf_len, recv_buf, recv_buf_capacity);
			line_buf_len += recv_len;
			break;
		} else {
			while (line_buf_len + line_feed_index + 1> line_buf_capacity) {
				line_buf_capacity *= 2;
				line_buf = realloc(line_buf, line_buf_capacity);
			}
			memcpy(line_buf + line_buf_len, recv_buf, line_feed_index + 1);
			line_buf[line_buf_len + line_feed_index + 1] = '\0';
			line_buf_len += line_feed_index + 1;
		}
	}

	// CR position
	line_buf[line_buf_len - 1] = '\0';

	log_debug("[%d] first line: %s\n", fd, line_buf);
	char *method;
	char *request_target;
	char *http_version;

	char *first_space_ptr = strchr(line_buf, ' ');
	*first_space_ptr = '\0';

	method = strdup(line_buf);

	char *second_space_ptr = strchr(first_space_ptr + 1, ' ');
	*second_space_ptr = '\0';

	request_target = strdup(first_space_ptr + 1);

	http_version = strdup(second_space_ptr + 1);

	log_debug("[%d] Request line\n\tMethod: %s\n\tRequest target: %s\n\tHTTP version: %s\n", data.fd, method, request_target, http_version);

	char *http_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, world!";
	int send_result =send(data.fd, http_response, strlen(http_response), 0);
	if (send_result == -1) {
		perror("send");
	}
	
	close(data.fd);
	return NULL;
}

int main(int argc, char **argv)
{
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (socket_fd == -1){
		perror("socket");
		exit(1);
	}

	int so_reuseaddr_enabled = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr_enabled, sizeof(int)) == -1) {
		perror("setsockpot");
	}

	struct sockaddr_in bind_addr;
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(8080);
	bind_addr.sin_addr.s_addr = 0;

	int bind_result = bind(socket_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr));

	if (bind_result == -1){
		perror("bind");
		exit(2);
	}

	int listen_result = listen(socket_fd, backlog);

	if(listen_result == -1){
		perror("listen");
		exit(3);
	}

	struct sockaddr_in accepted_socket_addr;
	socklen_t accepted_socket_length = sizeof(accepted_socket_addr);

	int count = 0;
	while(!server_stopped){
		printf("count: %d\n", count++);

		int accepted_socket_fd = accept(socket_fd, (struct sockaddr *)&accepted_socket_addr, &accepted_socket_length);

		if(accepted_socket_fd == -1) {
			perror("accept");
			exit(4);
		}

		log_debug("accepted: fd=%d, addr=%s:%d\n",
				accepted_socket_fd,
				inet_ntoa(accepted_socket_addr.sin_addr),
				accepted_socket_addr.sin_port);

		struct socket_handler_data *data = malloc(sizeof(struct socket_handler_data));
		data->fd = accepted_socket_fd;

		pthread_t thread;
		int pthread_result = pthread_create(&thread, NULL, accepted_socket_handler, data);

		if (pthread_result != 0) {
			perror("pthread_create");
			exit(5);
		}
	}
	return 0;
}
