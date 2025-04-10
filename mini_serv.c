#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/select.h>

typedef struct s_client {
  int id;
  char *message;
} t_client;

t_client clients[5000];
int last_fd, clients_count = 0;
char buff_receive[1001], buff_send[1001];
fd_set active_fds, write_fds, read_fds;

int extract_message(char **buf, char **msg) {
  char	*newbuf;
  int	i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i]) {
    if ((*buf)[i] == '\n') {
      newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));

      if (newbuf == 0)
        return (-1);

      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = 0;
      *buf = newbuf;

      return (1);
    }
    i++;
  }
  return (0);
}

char *str_join(char *buf, char *add) {
  char *newbuf;
  size_t len;

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

void panic() {
  write(STDERR_FILENO, "Fatal error\n", 12);
  exit(EXIT_FAILURE);
}

void notify_others(int author_fd, char *message) {
  for (int fd = 0; fd <= last_fd; ++fd) {
    if (FD_ISSET(fd, &write_fds) && fd != author_fd)
      send(fd, message, strlen(message), 0);
  }
}

void register_client(int fd) {
  last_fd = fd > last_fd ? fd : last_fd;
  clients[fd].id = clients_count++;
  clients[fd].message = NULL;
  FD_SET(fd, &active_fds);
  sprintf(buff_send, "server: client %d just arrived\n", clients[fd].id);
  notify_others(fd, buff_send);
}

void send_client_message(int fd) {
  char *message = NULL;

  while (extract_message(&clients[fd].message, &message)) {
    sprintf(buff_send, "client %d: ", clients[fd].id);
    notify_others(fd, buff_send);
    notify_others(fd, message);
    free(message);
    message = NULL;
  }
}

void unregister_client(int fd) {
  sprintf(buff_send, "server: client %d just left\n", clients[fd].id);
  notify_others(fd, buff_send);
  FD_CLR(fd, &active_fds);
  close(fd);

  if (clients[fd].message)
    free(clients[fd].message);
}

int main(int argc, char **argv) {
  int socket_fd, port, client_fd;
  struct sockaddr_in serv_addr, cli_addr;
  socklen_t serv_addr_len, cli_addr_len;

  if (argc < 2) {
    write(2, "Wrong number of arguments\n", 26);
    return EXIT_FAILURE;
  }

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0)
    panic();

  port = atoi(argv[1]);
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  serv_addr.sin_port = htons(port);

  serv_addr_len = sizeof(serv_addr);
  if (bind(socket_fd, (struct sockaddr *)&serv_addr, serv_addr_len) < 0)
    panic();

  if (listen(socket_fd, SOMAXCONN) < 0)
    panic();

  FD_ZERO(&active_fds);
  FD_SET(socket_fd, &active_fds);
  last_fd = socket_fd;

  while (1) {
    read_fds = write_fds = active_fds;

    if (select(last_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
      continue;

    for (int fd = 0; fd <= last_fd; ++fd) {
      if (!FD_ISSET(fd, &read_fds))
        continue;

      if (fd == socket_fd) {
        cli_addr_len = sizeof(cli_addr);
        client_fd = accept(fd, (struct sockaddr *)&cli_addr, &cli_addr_len);

        if (client_fd >= 0) {
          register_client(client_fd);
          break;
        }
      } else {
        size_t bytes_read = recv(fd, buff_receive, 1000, 0);

        if (bytes_read <= 0) {
          unregister_client(fd);
          break;
        }

        buff_receive[bytes_read] = '\0';
        clients[fd].message = str_join(clients[fd].message, buff_receive);
        send_client_message(fd);
      }
    }
  }
}
