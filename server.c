#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MYPORT "4000"
#define BACKLOG 10

void sigchild_handler(int s) {
  (void)s;
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  struct addrinfo hints, *res, *p;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;
  struct sigaction sa;
  int sockfd, new_fd;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, MYPORT, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    status = bind(sockfd, p->ai_addr, p->ai_addrlen);
    if (status < 0) {
      close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(res);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return 3;
  }

  status = listen(sockfd, BACKLOG);
  if (status < 0) {
    perror("listen");
    return 4;
  }

  sa.sa_handler = sigchild_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) < 0) {
    perror("sigaction");
    exit(1);
  }

  printf("server listening...\n");

  while (1) {
    sin_size = sizeof client_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (new_fd < 0) {
      perror("accept");
      continue;
    }

    inet_ntop(client_addr.ss_family,
              get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
    printf("Server got connection from %s\n", s);

    if (!fork()) {
      close(sockfd);
      if (send(new_fd, "Hello, world!", 13, 0) < 0) {
        perror("send");
      }

      close(new_fd);
      exit(0);
    }

    close(new_fd);
  }

  return 0;
}
