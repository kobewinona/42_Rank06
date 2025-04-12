#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct s_client {
  int id;
  char message[100000];
} t_client;

t_client clients[1024];

int last_fd = 0, clients_count = 0;
fd_set read_fds, write_fds, active_fds;
char recv_buffer[100000], send_buffer[100000];

void panic() {
  write(STDERR_FILENO, "Fatal error\n", 12);
  exit(EXIT_FAILURE);
}

void notify_others(int client_fd, char *message) {
  for (int fd = 3; fd <= last_fd; ++fd) {
    if (FD_ISSET(fd, &write_fds) && fd != client_fd)
      send(fd, message, strlen(message), 0);
  }
}

void register_client(int client_fd) {
  FD_SET(client_fd, &active_fds);
  clients[client_fd].id = clients_count++;
  bzero(&clients[client_fd].message, sizeof(clients[client_fd].message));
  sprintf(send_buffer, "server: client %d just arrived\n", clients[client_fd].id);
  notify_others(client_fd, send_buffer);
}

void unregister_client(int client_fd) {
  sprintf(send_buffer, "server: client %d just left\n", clients[client_fd].id);
  notify_others(client_fd, send_buffer);
  FD_CLR(client_fd, &active_fds);
  close(client_fd);
  bzero(clients[client_fd].message, sizeof(clients[client_fd].message));
}

void send_client_message(int client_fd, ssize_t bytes_read) {
  for (int i = 0, j = strlen(clients[client_fd].message); i < bytes_read; ++i, ++j) {
    clients[client_fd].message[j] = recv_buffer[i];

    if (clients[client_fd].message[j] == '\n') {
      clients[client_fd].message[j] = '\0';
      sprintf(send_buffer, "client %d: %s\n", clients[client_fd].id, clients[client_fd].message);
      notify_others(client_fd, send_buffer);
      bzero(clients[client_fd].message, sizeof(clients[client_fd].message));
      j = -1;
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    write(STDERR_FILENO, "Wrong number of arguments\n", 26);
    return EXIT_FAILURE;
  }

  int socket_fd, client_fd;
  struct sockaddr_in serv_addr;

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0)
    panic();

  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(2130706433);
  serv_addr.sin_port = htons(atoi(argv[1]));

  if (bind(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
    panic();

  if (listen(socket_fd, 10) != 0)
    panic();

  FD_ZERO(&read_fds), FD_ZERO(&write_fds), FD_ZERO(&active_fds);
  FD_SET(socket_fd, &active_fds);
  last_fd = socket_fd;

  while(1) {
    read_fds = write_fds = active_fds;

    if (select(last_fd + 1, &read_fds, &write_fds, NULL, NULL) <= 0)
      continue;

    if (FD_ISSET(socket_fd, &read_fds)) {
      struct sockaddr cli_addr;
      bzero(&cli_addr, sizeof(cli_addr));
      socklen_t cli_addr_len = sizeof(cli_addr);
      client_fd = accept(socket_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);

      if (client_fd >= 0) {
        if (client_fd > last_fd)
          last_fd = client_fd;

        register_client(client_fd);
        continue;
      }
    }

    for (int fd = 3; fd <= last_fd; ++fd) {
      if (!FD_ISSET(fd, &read_fds) || fd == socket_fd)
        continue;

      bzero(recv_buffer, sizeof(recv_buffer));
      ssize_t bytes_read = recv(fd, recv_buffer, 1000, 0);

      if (bytes_read <= 0)
        unregister_client(fd);
      else
        send_client_message(fd, bytes_read);
    }
  }
}
