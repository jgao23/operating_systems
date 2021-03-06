#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

struct details {
  long size;
  int status_code;
  char buff[10];
};

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */

void handle_files_request(int fd) {

  /* YOUR CODE HERE */

  struct http_request *request = http_request_parse(fd);

  char *path = request->path;
  int status_code = 0;
  char *content_type;
  FILE *fp;
  long size;
  char *file;
  char *index = "/index.html";

  unsigned int const size_direc = strlen(server_files_directory);
  unsigned int const size_path = strlen(path);
  unsigned int const size_index = strlen(index);
  char *total_path = (char*)malloc(size_direc+size_path+1);

  memcpy(total_path, server_files_directory, size_direc);
  memcpy(total_path + size_direc, path, size_path);
  total_path[size_path+size_direc] = '\0';

  struct stat file_details;
  if (stat(total_path, &file_details) == 0) {
    if (file_details.st_mode & S_IFREG) {
      printf("getting into a file");
      status_code = 200;
      size = file_details.st_size;
      char file_new[size+1];
      fp = fopen(total_path, "r");
      if (fp == NULL) {
        printf("Error");
      }
      fread(file_new, size, 1, fp);
      fclose(fp);

      fprintf(stdout, "gettisng here");
      char size_str[size+1];
      sprintf(size_str, "%ld", size);
      size_str[size] = '\0';

      content_type = http_get_mime_type(total_path);
      http_start_response(fd, status_code);
      http_send_header(fd, "Content-Type", content_type);
      http_send_header(fd, "Content-Length", size_str);
      http_end_headers(fd);
      http_send_data(fd, file_new, size);
    } 

    else if (file_details.st_mode & S_IFDIR) {
      char *test_index = (char*)malloc(size_direc+size_path+size_index);
      memcpy(test_index, total_path, size_direc+size_path);
      memcpy(test_index + size_direc + size_path, index, size_index);
      test_index[size_path + size_direc + size_index] = '\0';

      int exists = access(test_index, F_OK);
      if (exists != -1) {

        struct stat details;
        stat(test_index, &details);

        status_code = 200;
        size = details.st_size;

        file = malloc(size + 1);
        fp = fopen(test_index, "rb");
        fread(file, size, 1, fp);
        fclose(fp);
        file[size] = 0;

        char size_str[size+1];
        sprintf(size_str, "%ld", size);
        size_str[size] = '\0';

        content_type = http_get_mime_type(test_index);
        http_start_response(fd, status_code);
        http_send_header(fd, "Content-Type", content_type);
        http_send_header(fd, "Content-Length", size_str);
        http_end_headers(fd);
        http_send_data(fd, file, size);
        free(file);
        free(size_str);
      } else {
        status_code = 200;
        DIR *directory = opendir(total_path);
        struct dirent *in_dir;
        http_start_response(fd, status_code);
        http_send_header(fd, "Content-Type", "text/html"); 
        http_end_headers(fd);
        http_send_string(fd, "<a href=\"../\">Parent directory</a>\n");
        while ((in_dir = readdir(directory)) != NULL) {
          char ref[256];
          sprintf(ref, "<a href=\"%s\">%s</a>\n", in_dir->d_name, in_dir->d_name);
          http_send_string(fd, ref);
        }
        closedir(directory);
      }
    }
  } else {
    status_code = 404;
    http_start_response(fd, status_code);
  }
  free(total_path);
  close(fd);
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  struct hostent *ent = gethostbyname(server_proxy_hostname);
  struct in_addr **addr_list;
  char ip[100];
  addr_list = (struct in_addr **) ent->h_addr_list;
  strcpy(ip, inet_ntoa(*addr_list[0]));

  int socketfd;

  struct sockaddr_in server_address;

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(ip);
  server_address.sin_port = htons(server_proxy_port);

  socketfd = socket(PF_INET, SOCK_STREAM, 0);

  fd_set readfds;

  char buff[8192];

  connect(socketfd, (struct sockaddr*) &server_address, sizeof(server_address));

  FD_ZERO(&readfds);
  FD_SET(socketfd, &readfds);
  FD_SET(fd, &readfds);

  ssize_t read_size;
  int maxfd;
  if (fd > socketfd) {
    maxfd = fd;
  } else {
    maxfd = socketfd;
  }
  while(1) {
    select(maxfd + 1, &readfds, NULL, NULL, NULL);
    // check if fd client is ready to read from to send data to socketfd
    if (FD_ISSET(fd, &readfds)) {
      read_size = read(fd, buff, sizeof(buff));
      write(socketfd, buff, read_size);
    } else if (FD_ISSET(socketfd, &readfds)) {
      // check if socketfd is ready to read from to send data to fd
      read_size = read(socketfd, buff, sizeof(buff));
      write(fd, buff, read_size);
    }
    FD_ZERO(&readfds);
    FD_SET(socketfd, &readfds);
    FD_SET(fd, &readfds);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      // Un-register signal handler (only parent should have it)
      signal(SIGINT, SIG_DFL);
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      perror("Failed to fork child");
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
