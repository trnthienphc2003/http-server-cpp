#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
constexpr int BUF_LEN = 1024;

char *read_content_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == nullptr) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return nullptr;
  }
  
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  char *buffer = new char[file_size + 1];
  fread(buffer, 1, file_size, file);
  buffer[file_size] = '\0';
  
  fclose(file);
  
  return buffer;
}

int main(int argc, char **argv) {
  printf("Arguments: %s\n", argv[0]);
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  //
  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  pid_t child_pid;
  int client;
  while(1) {
    std::cout << "Waiting for a client to connect...\n";
    client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    
    if (client < 0) {
      std::cerr << "Failed to accept client connection\n";
      return 1;
    }

    if((child_pid = fork()) == 0) {
      close(server_fd);
      std::cout << "Client connected\n";
      char get_head[] = "GET ";
      char *buffer = new char[BUF_LEN];
      int bytes_received = recv(client, buffer, BUF_LEN, 0);

      char *urlpath = nullptr;
      urlpath = strstr(buffer, get_head);

      char *location_user_agent = nullptr;
      location_user_agent = strstr(buffer, "User-Agent: ");
      // printf("User-Agent: %s\n", location_user_agent);
      
      char delimiter[] = " ";
      
      char *path = nullptr;
      path = strtok(urlpath, delimiter);
      path = strtok(NULL, delimiter);
      printf("Path: %s\n", path);



      if(strcmp(path, "/") == 0) {
        // printf("[DEBUG]\n%s\n", buffer);
        // printf("User Agent: %s\n", location_user_agent);
        
        {
          std::cout << "Client request \n";
          send(client, "HTTP/1.1 200 OK\r\n\r\n", 20, 0);
        }
      }

      else if(strncmp("/echo/", path, 5) == 0) {
        std::cout << "Client requested echo\n";
        char *echo_string = nullptr;
        echo_string = strtok(path, "/");
        echo_string = strtok(NULL, "/");
        printf("Echo string: %s\n", echo_string);

        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(strlen(echo_string)) + "\r\n\r\n" + echo_string;
        send(client, response.data(), response.size(), 0);
      }

      else if(strncmp("/files/", path, 7) == 0) {
        std::cout << "Client requested file\n";
        char *file_name = nullptr;
        file_name = strtok(path, "/");
        file_name = strtok(NULL, "/");
        printf("File name: %s\n", file_name);
        char *file_content = read_content_file(file_name);
        printf("File content: %s\n", file_content);
        if(file_content) {
          std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(strlen(file_content)) + "\r\n\r\n" + file_content;
          send(client, response.data(), response.size(), 0);
          delete []file_content;
        }

        else {
          send(client, "HTTP/1.1 404 Not Found\r\n\r\n", 27, 0);
        }
      }

      else if(strcmp(path, "/user-agent") == 0) {
        if(strncmp(location_user_agent, "User-Agent: ", 12) == 0) {
          std::cout << "Client requested user-agent\n";
          // convert to stl c++ string
          char *user_agent = location_user_agent;
          user_agent += 12; // Move past "User-Agent: "
          char *end_of_user_agent = strstr(user_agent, "\r\n");
          *end_of_user_agent = '\0'; // Null-terminate the string
          std::string user_agent_str(user_agent);
          printf("User-Agent: %s\n", user_agent_str.c_str());
      
          std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(user_agent_str.size()) + "\r\n\r\n" + user_agent_str;
          send(client, response.data(), response.size(), 0);
        }
      }

      else {
        std::cout << "Client requested something else\n";
        send(client, "HTTP/1.1 404 Not Found\r\n\r\n", 27, 0);
      }

      
      // send(client, "HTTP/1.1 200 OK\r\n\r\n", 20, 0);
      
      delete []buffer;
    }
    
  }
  close(client); 
  // delete []urlpath;
  // delete []path;


  return 0;
}
