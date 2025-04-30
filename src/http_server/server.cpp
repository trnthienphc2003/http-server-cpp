#include <http_server/server.hpp>
#include <http_server/request.hpp>
#include <http_server/response.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <fstream>
#include <cstring>
#include <sstream>          // std::istringstream
#include <thread>           // std::thread<
#include <unistd.h>         // close()
#include <filesystem>       // std::filesystem
#include <arpa/inet.h>      // sockaddr_in, htons(), INADDR_ANY

http_server::HTTP_Server::HTTP_Server(uint16_t port, std::string root_path) {
    try {
        // Create socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(server_fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        // Set socket options to reuse address and port
        int opt = 1;
        if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("Failed to set socket options");
        }
        
        // Setup server address
        std::memset(&this->server_address, 0, sizeof(this->server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port);
        
        // Bind socket
        if(bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            throw std::runtime_error("Failed to bind to port " + std::to_string(port));
        }
        
        // Listen for connections
        int connection_backlog = 5;
        if(listen(server_fd, connection_backlog) < 0) {
            throw std::runtime_error("Failed to listen on port " + std::to_string(port));
        }
        
        std::cout << "Server initialized on port " << port << std::endl;
    } catch (const std::exception& e) {
        // Close socket if it was opened
        if(server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        std::cerr << "Server initialization error: " << e.what() << std::endl;
        throw;  // Re-throw to be handled by main()
    }
}

void http_server::HTTP_Server::add_route(const std::string &method, const std::string &path_pattern, Handler handler) {
    try {
        // Compile the path pattern into a regex
        auto [regex_pattern, path_params] = Router::compile_path_pattern(path_pattern);
        
        // Add the route to the router
        this->router.add_route(method, path_pattern, handler);
    } catch (const std::exception& e) {
        std::cerr << "Error adding route: " << e.what() << std::endl;
        throw; // Re-throw to be handled by the caller
    }
}

void http_server::HTTP_Server::run() {
    try {
        std::cout << "Server starting to listen for connections..." << std::endl;
        
        while(true) {
            try {
                struct sockaddr_in client_address;
                int client_address_len = sizeof(client_address);
                
                int client_fd = accept(this->server_fd, (struct sockaddr *)&client_address, (socklen_t*)&client_address_len);
                if(client_fd < 0) {
                    throw std::runtime_error("Accept failed: " + std::string(strerror(errno)));
                }
                
                // Create a detached thread to handle the client
                std::thread([this, client_fd, client_address]() {
                    try {
                        handle_client_connection(client_fd, client_address);
                    } catch (const std::exception& e) {
                        std::cerr << "Error handling client: " << e.what() << std::endl;
                    }
                    // Ensure the client socket is closed even if an exception occurs
                    shutdown(client_fd, SHUT_RDWR);
                    close(client_fd);
                }).detach();
            } catch (const std::exception& e) {
                std::cerr << "Error accepting connection: " << e.what() << std::endl;
                // Continue to accept other connections even if one fails
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        throw; // Re-throw to be handled by main()
    }
}

void http_server::HTTP_Server::handle_client_connection(int client_fd, const sockaddr_in& client_address) {
    char buffer[BUF_LEN];
    std::string raw_request;
    bool keep_alive = true;
    
    // Get client IP for logging
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "New client connection from " << client_ip << std::endl;
    
    int requests = 0;
    while(keep_alive) {
        try {
            // Clear previous data
            memset(buffer, 0, BUF_LEN);
            
            // Receive data
            int bytes_read = recv(client_fd, buffer, BUF_LEN - 1, 0);
            if(bytes_read <= 0) {
                // Client disconnected or error
                if (bytes_read < 0) {
                    std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                }
                break;
            }
            
            raw_request = std::string(buffer, bytes_read);
            
            // Parse and dispatch the request
            HTTP_Request request;
            try {
                request = parse_request(raw_request);
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse request: " << e.what() << std::endl;
                // Send bad request response
                HTTP_Response error_response {
                    (int)HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {{"Connection", "close"}},
                    "Invalid HTTP request format"
                };
                std::string response_str = error_response.to_string();
                send(client_fd, response_str.c_str(), response_str.length(), 0);
                break; // Close connection on parse error
            }
            
            // Process the request
            HTTP_Response response;
            try {
                response = this->router.dispatch(request);
            } catch (const std::exception& e) {
                std::cerr << "Error dispatching request: " << e.what() << std::endl;
                response = HTTP_Response {
                    (int)HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
                    "Internal Server Error",
                    {},
                    "An error occurred while processing your request"
                };
            }
            
            // Check if keep-alive
            keep_alive = false;
            if(request.version == "HTTP/1.1") {
                auto it = request.headers.find("Connection");
                keep_alive = ((it == request.headers.end()) || (it->second != "close"));
            } else if(request.version == "HTTP/1.0") {
                auto it = request.headers.find("Connection");
                keep_alive = ((it != request.headers.end()) && (it->second == "keep-alive"));
            }
            
            // Set Connection header in response accordingly
            if(keep_alive) {
                response.headers["Connection"] = "keep-alive";
            } else {
                response.headers["Connection"] = "close";
            }
            
            // Send response
            std::string response_str = response.to_string();
            if (send(client_fd, response_str.c_str(), response_str.length(), 0) < 0) {
                std::cerr << "Error sending response: " << strerror(errno) << std::endl;
                break;
            }
            
            // Log the request
            std::cout << client_ip << " - " << request.method << " " << request.path 
                      << " - " << response.status_code << std::endl;
            
            if(!keep_alive || ++requests >= http_server::config::MAX_KEEP_ALIVE_REQUESTS) {
                break;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing request from " << client_ip << ": " << e.what() << std::endl;
            
            // Send error response
            HTTP_Response error_response {
                (int)HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
                "Internal Server Error",
                {{"Connection", "close"}},
                "An error occurred while processing your request"
            };
            
            std::string response_str = error_response.to_string();
            send(client_fd, response_str.c_str(), response_str.length(), 0);
            break; // Close connection on error
        }
    }
}

http_server::HTTP_Server::~HTTP_Server() {
    try {
        if(server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error closing server socket: " << e.what() << std::endl;
    }
}
