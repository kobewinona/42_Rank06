#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

int client_ids[1024];
char *client_messages[1024];

int last_fd = 0, clients_count = 0;
fd_set read_fds, write_fds, active_fds;
char recv_buffer[1001], send_buffer[1001];

int extract_message(char **buf, char **msg)
{
  char	*newbuf;
  int	i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i])
  {
    if ((*buf)[i] == '\n')
    {
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

void panic() {
  write(STDERR_FILENO, "Fatal error\n", 12);
  exit(EXIT_FAILURE);
}

void notify_others(int client_fd, char *message) {
  for (int fd = 0; fd <= last_fd; fd++) {
    if (FD_ISSET(fd, &write_fds) && fd != client_fd) {
      ssize_t bytes_read = send(fd, message, strlen(message), 0);

      if (bytes_read <= 0) {
        FD_CLR(client_fd, &active_fds);
        close(client_fd);
      }
    }
  }
}

void register_client(int client_fd) {
  FD_SET(client_fd, &active_fds);
  client_ids[client_fd] = clients_count++;
  client_messages[client_fd] = NULL;
  sprintf(send_buffer, "server: client %d just arrived\n", client_ids[client_fd]);
  notify_others(client_fd, send_buffer);
  bzero(send_buffer, sizeof(send_buffer));
}

void unregister_client(int client_fd) {
  sprintf(send_buffer, "server: client %d just left\n", client_ids[client_fd]);
  notify_others(client_fd, send_buffer);
  bzero(send_buffer, sizeof(send_buffer));
  FD_CLR(client_fd, &active_fds);
  close(client_fd);
  free(client_messages[client_fd]);
}

void send_client_message(int client_fd) {
  char *message = NULL;

  while (extract_message(&client_messages[client_fd], &message)) {
    sprintf(send_buffer, "client %d: ", client_ids[client_fd]);
    strcat(send_buffer, message);
    notify_others(client_fd, send_buffer);
    bzero(send_buffer, sizeof(send_buffer));
    free(message);
    message = NULL;
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    write(STDERR_FILENO, "Wrong number of arguments\n", 26);
    return EXIT_FAILURE;
  }

  int socket_fd, client_fd;
  struct sockaddr_in serv_addr, cli_addr;

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0)
    panic();

  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(2130706433);
  serv_addr.sin_port = htons(atoi(argv[1]));

  if (bind(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
    panic();

  if (listen(socket_fd, SOMAXCONN) != 0)
    panic();

  FD_ZERO(&active_fds);
  FD_SET(socket_fd, &active_fds);
  last_fd = socket_fd;
  socklen_t cli_addr_len = sizeof(serv_addr);

  while(1) {
    read_fds = write_fds = active_fds;

    if (select(last_fd + 1, &read_fds, &write_fds, NULL, NULL) <= 0)
      continue;

    for (int fd = 0; fd <= last_fd; fd++) {
      if (!FD_ISSET(fd, &read_fds))
        continue;

      if (fd == socket_fd) {
        client_fd = accept(socket_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);

        if (client_fd >= 0) {
          if (client_fd > last_fd)
            last_fd = client_fd;

          register_client(client_fd);
          break;
        }
      } else {
        bzero(recv_buffer, sizeof(recv_buffer));
        int bytes_read = recv(fd, recv_buffer, 1000, 0);

        if (bytes_read <= 0) {
          unregister_client(fd);
        } else {
          recv_buffer[bytes_read] = '\0';
          client_messages[fd] = str_join(client_messages[fd], recv_buffer);
          send_client_message(fd);
        };
      }
    }
  }
}
