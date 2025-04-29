#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <map>
#include <functional>
#include <optional>
#include <thread>
#include <fstream>
#include <sstream>
constexpr int BUF_LEN = 1024;

enum class HTTP_STATUS_CODE{
  OK = 200,
  CREATED = 201,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500,
  BAD_REQUEST = 400,
  FORBIDDEN = 403,
};
struct HTTP_Request {
  std::string method, path, version, body;
  std::map<std::string, std::string> headers;
};

struct HTTP_Response {
  int status_code;
  std::string status_message;
  std::map<std::string, std::string> headers;
  std::string body;

  std::string to_string() const {
    /*
      For serialization purpose
      Convert the response to a string
    */

    std::string response;
    if(status_code == (int)HTTP_STATUS_CODE::NOT_FOUND) {
      response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    else {
      response += "HTTP/1.1 " + std::to_string(status_code) + " " + status_message + "\r\n";
      for(const auto &[key, value]: headers) {
        response += key + ": " + value + "\r\n";
      }
      response += "\r\n";
      response += body;
    }
    
    return response;
  }
};

class HTTP_Server {
public:
  using Handler = std::function<HTTP_Response(const HTTP_Request &)>;
  HTTP_Server(uint16_t port) {
    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(this->server_fd < 0) {
      std::cerr << "Failed to create server socket\n";
      exit(1);
    }

    int yes = 1;
    if(setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
      std::cerr << "setsockopt failed\n";
      exit(1);
    }

    std::memset(&this->server_address, 0, sizeof(this->server_address));
    this->server_address.sin_family = AF_INET;
    this->server_address.sin_addr.s_addr = INADDR_ANY;
    this->server_address.sin_port = htons(port);

    if(bind(this->server_fd, (struct sockaddr *)&this->server_address, sizeof(this->server_address)) < 0) {
      std::cerr << "Failed to bind to port " << port << "\n";
      exit(1);
    }

    int connection_backlog = 5;
    if(listen(this->server_fd, connection_backlog) < 0) {
      std::cerr << "Failed to listen on port " << port << "\n";
      exit(1);
    }
  };
  void route(const std::string &path_pattern, Handler handler) {
    /*
      Register a route with the server
      path_pattern: The path pattern to match
      handler: The function to call when the route is matched
    */
    std::cout << "Registering route: " << path_pattern << "\n";
    routes.emplace_back(path_pattern, std::move(handler));
  }
  void run() {
    /*
      Main execution loop of the server
      Accept incoming connections and handle requests
      This function should be blocking and run indefinitely
    */
    while(true) {
      struct sockaddr_in client_address;
      int client_address_len = sizeof(client_address);
      int client = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t *)&client_address_len);

      // Handle concurrency using thread
      std::thread([this, client]() {
        // Reading requests
        // std::string raw_request(BUF_LEN, '\0');
        char buffer[BUF_LEN];
        int bytes_received = recv(client, buffer, BUF_LEN, 0);
        if(bytes_received <= 0) {
          close(client);
          return;
        }
        std::string raw_request(buffer, static_cast<size_t>(bytes_received));

        HTTP_Request request = parse_request(raw_request);
        HTTP_Response response = dispatch(request);
        std::string response_str = response.to_string();

        // Send response
        send(client, response_str.data(), response_str.size(), 0);
        // Close the client socket
        close(client);
      }).detach();
    }
  }
  std::optional<std::string> read_file_interface(const std::string& file_path) {
    std::cout << "Reading file: " << file_path << "\n";
    return this->read_file(file_path);
  }

  void save_file(const std::string &file_path, const std::string &content) {
    /*
      Save the content to a file
      This function should handle file writing
    */
    std::ofstream file(file_path, std::ios::binary);
    if(!file) {
      std::cerr << "Failed to open file for writing: " << file_path << "\n";
      return;
    }
    file.write(content.data(), content.size());
    if(!file) {
      std::cerr << "Failed to write to file: " << file_path << "\n";
    }
    file.close();
  }
  ~HTTP_Server() {
    /*
      Destructor to clean up resources
      This function should close the server socket
    */
    close(server_fd);
  };
private:
  int server_fd;
  struct sockaddr_in server_address;
  std::vector <std::pair<std::string, Handler>> routes;
  HTTP_Request parse_request(const std::string &raw) {
    HTTP_Request request;
    /*
      Parse the raw HTTP request string into an HTTP_Request object
      This function should handle the request line and headers
    */

    // Split headers and body
    size_t sep = raw.find("\r\n\r\n");
    if(sep == std::string::npos) {
      sep = 0;
    }
    std::string headers = raw.substr(0, sep);
    std::string body = (raw.size() > sep + 4 ? raw.substr(sep + 4) : "");
    request.body = body;

    // Split header block into lines
    std::vector <std::string> lines;
    size_t start = 0;
    size_t end;
    while((end = headers.find("\r\n", start)) != std::string::npos) {
      lines.push_back(headers.substr(start, end - start));
      start = end + 2;
    }

    if(start < (int)headers.size()) { 
      lines.push_back(headers.substr(start));
    }

    if(lines.empty()) {
      // throw std::runtime_error("Invalid request");
      return request;
    }

    // Parse request line
    {
      std::istringstream request_line(lines[0]);
      request_line >> request.method >> request.path >> request.version;  
    }

    // Parse headers
    for(int i = 1; i < (int)lines.size(); ++i) {
      size_t sep = lines[i].find(": ");
      if(sep == std::string::npos) {
        // throw std::runtime_error("Invalid header");
        continue;
      }

      request.headers[
        lines[i].substr(0, sep)
      ] = lines[i].substr(sep + 2);
    }

    return request;
  }
  HTTP_Response dispatch(const HTTP_Request &request) {
    HTTP_Response response;
    /*
      Dispatch the request to the appropriate handler based on the path
      This function should iterate over the registered routes and call the matching handler
    */
    for(const auto &[pattern, handler]: routes) {
      if(request.path == pattern) {
        return handler(request);
      }

      if(!pattern.empty() && pattern != "/" && pattern.back() == '/' && request.path.rfind(pattern) == 0) {
        std::cout << "Client requested path: " << request.path << "\n";
        std::cout << "Matching route: " << pattern << "\n";
        return handler(request);
      }
    }

    return {
      404,
      "Not Found",
    };
  }
  std::optional<std::string> read_file(const std::string &file_path) {
    std::ifstream file{file_path, std::ios::binary};
    if(!file) {
      return std::nullopt;
    }

    return {
      std::string {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
      }
    };
  }
};

int main(int argc, char **argv) {
  // std::string root_path = argv[2];
  std::string root_path = (argc > 2 ? argv[2] : ".");
  HTTP_Server server(4221);

  server.route("/", [](const HTTP_Request &request) {
    return HTTP_Response {
      (int)HTTP_STATUS_CODE::OK,
      "OK",
      {},
      "",
    };
  });

  server.route("/echo/", [&](const HTTP_Request &request) {
    std::cout << "Client requested echo\n";

    std::string msg = request.path.substr(6);
    // std::cout << "Message: " << msg << "\n";
    return HTTP_Response {
      (int)HTTP_STATUS_CODE::OK,
      "OK",
      {{
        "Content-Type", "text/plain"
      },
      {
        "Content-Length", std::to_string(msg.size())
      }},
      msg
    };
  });

  server.route("/files/", [&](const HTTP_Request &request){
    std::string name = request.path.substr(7);
    if(request.method == "GET") {
      std::string file_path = root_path + "/" + name;
      if(auto content = server.read_file_interface(file_path)) {
        return HTTP_Response {
          (int)HTTP_STATUS_CODE::OK,
          "OK",
          {{
            "Content-Type", "application/octet-stream"
          },
          {
            "Content-Length", std::to_string(content->size())
          }},
          *content
        };
      }
    }

    else if(request.method == "POST") {
      std::string file_path = root_path + "/" + name;
      std::string content = request.body;

      server.save_file(file_path, content);
      return HTTP_Response {
        (int)HTTP_STATUS_CODE::CREATED,
        "Created",
        {},
        "",
      };
    }
    

    return HTTP_Response {
      (int)HTTP_STATUS_CODE::NOT_FOUND,
      "Not Found",
      {},
      "",
    };
  });

  server.route("/user-agent", [&](const HTTP_Request &request) {
    auto it = request.headers.find("User-Agent");
    if(it != request.headers.end()) {
      return HTTP_Response {
        (int)HTTP_STATUS_CODE::OK,
        "OK",
        {{
          "Content-Type", "text/plain"
        },
        {
          "Content-Length", std::to_string(it->second.size())
        }},
        it->second
      };
    }

    return HTTP_Response {
      (int)HTTP_STATUS_CODE::BAD_REQUEST,
      "Bad Request",
      {},
      "",
    };
  });

  server.run();
}

// char *read_content_file(const char *filename) {
//   FILE *file = fopen(filename, "r");
//   if (file == nullptr) {
//     std::cerr << "Failed to open file: " << filename << std::endl;
//     return nullptr;
//   }
  
//   fseek(file, 0, SEEK_END);
//   long file_size = ftell(file);
//   fseek(file, 0, SEEK_SET);
  
//   char *buffer = new char[file_size + 1];
//   fread(buffer, 1, file_size, file);
//   buffer[file_size] = '\0';
  
//   fclose(file);
  
//   return buffer;
// }

// int main(int argc, char **argv) {
//   // printf("Arguments: %s\n", argv[2]);
//   // Flush after every std::cout / std::cerr
//   std::cout << std::unitbuf;
//   std::cerr << std::unitbuf;
  
//   // You can use print statements as follows for debugging, they'll be visible when running tests.
//   std::cout << "Logs from your program will appear here!\n";

//   // Uncomment this block to pass the first stage
//   //
//   int server_fd = socket(AF_INET, SOCK_STREAM, 0);
//   if (server_fd < 0) {
//    std::cerr << "Failed to create server socket\n";
//    return 1;
//   }
//   //
//   // // Since the tester restarts your program quite often, setting SO_REUSEADDR
//   // // ensures that we don't run into 'Address already in use' errors
//   int reuse = 1;
//   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
//     std::cerr << "setsockopt failed\n";
//     return 1;
//   }
  
//   struct sockaddr_in server_addr;
//   server_addr.sin_family = AF_INET;
//   server_addr.sin_addr.s_addr = INADDR_ANY;
//   server_addr.sin_port = htons(4221);
  
//   if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
//     std::cerr << "Failed to bind to port 4221\n";
//     return 1;
//   }
  
//   int connection_backlog = 5;
//   if (listen(server_fd, connection_backlog) != 0) {
//     std::cerr << "listen failed\n";
//     return 1;
//   }
  
//   struct sockaddr_in client_addr;
//   int client_addr_len = sizeof(client_addr);
  
//   pid_t child_pid;
//   int client;
//   while(1) {
//     std::cout << "Waiting for a client to connect...\n";
//     client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    
//     if (client < 0) {
//       std::cerr << "Failed to accept client connection\n";
//       return 1;
//     }

//     if((child_pid = fork()) == 0) {
//       close(server_fd);
//       std::cout << "Client connected\n";
//       char get_head[] = "GET ";
//       char *buffer = new char[BUF_LEN];
//       int bytes_received = recv(client, buffer, BUF_LEN, 0);

//       char *urlpath = nullptr;
//       urlpath = strstr(buffer, get_head);

//       char *location_user_agent = nullptr;
//       location_user_agent = strstr(buffer, "User-Agent: ");
//       // printf("User-Agent: %s\n", location_user_agent);
      
//       char delimiter[] = " ";
      
//       char *path = nullptr;
//       path = strtok(urlpath, delimiter);
//       path = strtok(NULL, delimiter);
//       printf("Path: %s\n", path);



//       if(strcmp(path, "/") == 0) {
//         // printf("[DEBUG]\n%s\n", buffer);
//         // printf("User Agent: %s\n", location_user_agent);
        
//         {
//           std::cout << "Client request \n";
//           send(client, "HTTP/1.1 200 OK\r\n\r\n", 20, 0);
//         }
//       }

//       else if(strncmp("/echo/", path, 5) == 0) {
//         std::cout << "Client requested echo\n";
//         char *echo_string = nullptr;
//         echo_string = strtok(path, "/");
//         echo_string = strtok(NULL, "/");
//         printf("Echo string: %s\n", echo_string);

//         std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(strlen(echo_string)) + "\r\n\r\n" + echo_string;
//         send(client, response.data(), response.size(), 0);
//       }

//       else if(strncmp("/files/", path, 7) == 0) {
//         std::cout << "Client requested file\n";

//         char *file_name = nullptr;
//         file_name = strtok(path, "/");
//         file_name = strtok(NULL, "/");
//         printf("File name: %s\n", file_name);

//         std::string root_path = argv[2];
//         std::string file_path = root_path + "/" + file_name;

//         char *file_content = read_content_file(file_path.c_str());
//         printf("File content: %s\n", file_content);
//         if(file_content) {
//           std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(strlen(file_content)) + "\r\n\r\n" + file_content;
//           send(client, response.data(), response.size(), 0);
//           delete []file_content;
//         }

//         else {
//           send(client, "HTTP/1.1 404 Not Found\r\n\r\n", 27, 0);
//         }
//       }

//       else if(strcmp(path, "/user-agent") == 0) {
//         if(strncmp(location_user_agent, "User-Agent: ", 12) == 0) {
//           std::cout << "Client requested user-agent\n";
//           // convert to stl c++ string
//           char *user_agent = location_user_agent;
//           user_agent += 12; // Move past "User-Agent: "
//           char *end_of_user_agent = strstr(user_agent, "\r\n");
//           *end_of_user_agent = '\0'; // Null-terminate the string
//           std::string user_agent_str(user_agent);
//           printf("User-Agent: %s\n", user_agent_str.c_str());
      
//           std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(user_agent_str.size()) + "\r\n\r\n" + user_agent_str;
//           send(client, response.data(), response.size(), 0);
//         }
//       }

//       else {
//         std::cout << "Client requested something else\n";
//         send(client, "HTTP/1.1 404 Not Found\r\n\r\n", 27, 0);
//       }

      
//       // send(client, "HTTP/1.1 200 OK\r\n\r\n", 20, 0);
      
//       delete []buffer;
//     }
    
//   }
//   close(client); 
//   // delete []urlpath;
//   // delete []path;


//   return 0;
// }
