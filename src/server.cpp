#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <fstream>
#include <cstring>
#include <sstream>          // std::istringstream
#include <thread>           // std::thread
#include <unistd.h>         // close()
#include <filesystem>       // std::filesystem


// Methods of HTTP_Response
std::string HTTP_Response::to_string() const {
    try {
        std::stringstream ss;
        
        // Status line
        ss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
        
        // Set Content-Length header if not already set
        std::map<std::string, std::string> all_headers = headers;
        if(all_headers.find("Content-Length") == all_headers.end() && !body.empty()) {
            all_headers["Content-Length"] = std::to_string(body.size());
        }
        
        // Headers
        for(const auto &[name, value] : all_headers) {
            ss << name << ": " << value << "\r\n";
        }
        
        // Empty line separator
        ss << "\r\n";
        
        // Body
        if(!body.empty()) {
            ss << body;
        }
        
        return ss.str();
    } catch (const std::exception& e) {
        std::cerr << "Error generating HTTP response: " << e.what() << std::endl;
        
        // Return a minimal valid HTTP response as fallback
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error";
    }
}

HTTP_Server::HTTP_Server(uint16_t port) {
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

void HTTP_Server::route(const std::string &path_pattern, Handler handler) {
    /*
        Register a route with the server
        path_pattern: The path pattern to match
        handler: The function to call when the route is matched
    */
    std::cout << "Registering route: " << path_pattern << "\n";
    routes.emplace_back(path_pattern, std::move(handler));
}

void HTTP_Server::run() {
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

void HTTP_Server::handle_client_connection(int client_fd, const sockaddr_in& client_address) {
    char buffer[BUF_LEN];
    std::string raw_request;
    bool keep_alive = true;
    
    // Get client IP for logging
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "New client connection from " << client_ip << std::endl;
    
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
                response = dispatch(request);
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
            
            if(!keep_alive) {
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

std::optional<std::string> HTTP_Server::read_file_interface(const std::string& file_path) {
    std::cout << "Reading file: " << file_path << "\n";
    return this->read_file(file_path);
}

bool HTTP_Server::is_path_inside_directory(const std::filesystem::path& path, const std::filesystem::path& directory) {
    // Get canonical absolute paths
    std::filesystem::path abs_directory = std::filesystem::canonical(directory);
    std::filesystem::path abs_path;
    
    try {
        abs_path = std::filesystem::canonical(path);
    } catch(const std::filesystem::filesystem_error&) {
        // If the path doesn't exist, check if its parent directory is valid and inside the root
        auto parent_path = path.parent_path();
        if (!std::filesystem::exists(parent_path)) {
            return false;
        }
        
        auto canonical_parent = std::filesystem::canonical(parent_path);
        // Check if the parent directory is within the root directory
        for (auto it1 = canonical_parent.begin(), it2 = abs_directory.begin();
             it1 != canonical_parent.end() && it2 != abs_directory.end();
             ++it1, ++it2) {
            if (*it1 != *it2) {
                return false;
            }
        }
        return true;
    }
    
    // Compare paths to check if abs_path starts with abs_directory
    auto it1 = abs_path.begin();
    auto it2 = abs_directory.begin();
    
    while (it2 != abs_directory.end()) {
        if (it1 == abs_path.end() || *it1 != *it2) {
            return false;
        }
        ++it1;
        ++it2;
    }
    
    return true;
}

std::string HTTP_Server::validate_file_path(const std::string& directory_root, const std::string& requested_path) {
    try {
        // Create the full path by appending the requested path to the root directory
        std::filesystem::path root_path = directory_root;
        std::filesystem::path rel_path = requested_path;
        std::filesystem::path full_path = root_path / rel_path;
        
        // Validate that the path is inside the root directory
        if (!is_path_inside_directory(full_path, root_path)) {
            throw std::runtime_error("Directory traversal detected: Access denied");
        }
        
        return full_path.string();
    } catch (const std::exception& e) {
        std::cerr << "Path validation error: " << e.what() << std::endl;
        throw; // Rethrow to be handled by the caller
    }
}

void HTTP_Server::save_file(const std::string &file_path, const std::string &content) {
    try {
        // Validate the file path
        std::filesystem::path validated_path = file_path;
        std::filesystem::path parent_path = validated_path.parent_path();
        
        // Create directories if they don't exist
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path);
        }
        
        // Write to file
        std::ofstream file(file_path, std::ios::binary);
        if(!file) {
            throw std::runtime_error("Failed to open file for writing: " + file_path);
        }
        file.write(content.data(), content.size());
        if(!file) {
            throw std::runtime_error("Failed to write to file: " + file_path);
        }
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Error saving file: " << e.what() << std::endl;
        throw; // Rethrow to be handled by the caller
    }
}

HTTP_Server::~HTTP_Server() {
    try {
        if(server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error closing server socket: " << e.what() << std::endl;
    }
}

HTTP_Request HTTP_Server::parse_request(const std::string &raw) {
    try {
        HTTP_Request request;
        
        // Split headers and body
        size_t sep = raw.find("\r\n\r\n");
        if(sep == std::string::npos) {
            throw std::runtime_error("Invalid request format: missing header/body separator");
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

        if(start < headers.size()) { 
            lines.push_back(headers.substr(start));
        }

        if(lines.empty()) {
            throw std::runtime_error("Invalid request: no headers found");
        }

        {
            // Parse request line
            std::istringstream request_line(lines[0]);
            if (!(request_line >> request.method >> request.path >> request.version)) {
                throw std::runtime_error("Invalid request line format");
            }
            
            // Validate HTTP method
            if (request.method.empty()) {
                throw std::runtime_error("Missing HTTP method");
            }
            
            // Validate HTTP path
            if (request.path.empty()) {
                throw std::runtime_error("Missing request path");
            }
            
            // Validate HTTP version
            if (request.version.empty()) {
                throw std::runtime_error("Missing HTTP version");
            }
        }

        // Parse headers
        for(int i = 1; i < (int)lines.size(); ++i) {
            size_t sep = lines[i].find(": ");
            if(sep == std::string::npos) {
                // Skip malformed headers but log them
                std::cerr << "Skipping malformed header: " << lines[i] << std::endl;
                continue;
            }

            std::string name = lines[i].substr(0, sep);
            std::string value = lines[i].substr(sep + 2);
            request.headers[name] = value;
        }

        // Process Accept-Encoding header for gzip support
        if (request.headers.find("Accept-Encoding") != request.headers.end()) {
            request.encoding_scheme = "";
            // Clean the header by removing spaces
            std::string& encoding_header = request.headers["Accept-Encoding"];
            encoding_header.erase(
                std::remove(encoding_header.begin(), encoding_header.end(), ' '),
                encoding_header.end()
            );

            std::istringstream encodings(encoding_header);
            std::string encoding;
            while(std::getline(encodings, encoding, ',')) {
                if(encoding == "gzip") {
                    request.encoding_scheme = encoding;
                    break;
                }
            }
        } else {
            request.encoding_scheme = "";
        }
        
        // Process Content-Length if present
        auto content_length_it = request.headers.find("Content-Length");
        if(content_length_it != request.headers.end()) {
            try {
                size_t content_length = std::stoul(content_length_it->second);
                if (content_length > request.body.size()) {
                    std::cerr << "Warning: Content-Length (" << content_length << ") exceeds actual body size (" 
                              << request.body.size() << ")" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Invalid Content-Length header: " << e.what() << std::endl;
            }
        }
        
        return request;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing HTTP request: " << e.what() << std::endl;
        throw; // Rethrow to be handled by caller
    }
}

HTTP_Response HTTP_Server::dispatch(const HTTP_Request &request) {
    try {
        // Check for exact path matches first
        for(const auto &[pattern, handler]: routes) {
            if(request.path == pattern) {
                try {
                    return handler(request);
                } catch (const std::exception& e) {
                    std::cerr << "Error in handler for route " << pattern << ": " << e.what() << std::endl;
                    throw;
                }
            }
        }

        // Check for prefix matches
        for(const auto &[pattern, handler]: routes) {
            if(!pattern.empty() && pattern != "/" && pattern.back() == '/' && request.path.rfind(pattern) == 0) {
                std::cout << "Client requested path: " << request.path << std::endl;
                std::cout << "Matching route: " << pattern << std::endl;
                try {
                    return handler(request);
                } catch (const std::exception& e) {
                    std::cerr << "Error in handler for route " << pattern << ": " << e.what() << std::endl;
                    throw;
                }
            }
        }

        // No matching route
        return {
            (int)HTTP_STATUS_CODE::NOT_FOUND,
            "Not Found",
            {},
            "Resource not found"
        };
    } catch (const std::exception& e) {
        std::cerr << "Error dispatching request to " << request.path << ": " << e.what() << std::endl;
        return HTTP_Response {
            (int)HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
            "Internal Server Error",
            {},
            "An error occurred while processing your request"
        };
    }
}

std::optional<std::string> HTTP_Server::read_file(const std::string &file_path) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(file_path)) {
            return std::nullopt;
        }
        
        std::ifstream file{file_path, std::ios::binary};
        if(!file) {
            throw std::runtime_error("Failed to open file for reading: " + file_path);
        }

        return {
            std::string {
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            }
        };
    } catch (const std::exception& e) {
        std::cerr << "Error reading file: " << e.what() << std::endl;
        return std::nullopt;
    }
}