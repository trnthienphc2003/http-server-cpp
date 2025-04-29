#include "server.hpp"
#include <stdexcept>
#include <zlib.h>
#include <filesystem>

static std::string compress_gzip(std::string &data) {
    try {
        z_stream zstream{};
        if(deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            throw std::runtime_error("Failed to initialize zlib");
        }

        zstream.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zstream.avail_in  = data.size();

        std::string out;
        out.reserve(data.size() / 2);  // heuristic

        char buffer[GZIP_BUF_LEN];
        int ret;
        do {
            zstream.next_out  = reinterpret_cast<Bytef*>(buffer);
            zstream.avail_out = sizeof(buffer);

            ret = deflate(&zstream, Z_FINISH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                deflateEnd(&zstream);
                throw std::runtime_error("Deflate failed: " + std::string(zstream.msg ? zstream.msg : "unknown error"));
            }

            std::size_t have = sizeof(buffer) - zstream.avail_out;
            out.append(buffer, have);
        } while (ret != Z_STREAM_END);

        deflateEnd(&zstream);
        return out;
    } catch (const std::exception& e) {
        std::cerr << "Compression error: " << e.what() << std::endl;
        // In case of compression failure, return the original data
        // This is safer than returning an empty string
        return data;
    }
}

int main(int argc, char **argv) {
    try {
        // Parse command line arguments with better error handling
        uint16_t port = 4221; // Default port
        std::string root_path = "."; // Default directory
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--directory" || arg == "-d") {
                if (i + 1 < argc) {
                    root_path = argv[++i];
                } else {
                    throw std::invalid_argument("--directory option requires a path argument");
                }
            } else if (arg.find("--port=") == 0) {
                try {
                    std::string port_str = arg.substr(7);
                    port = std::stoi(port_str);
                    if (port <= 0 || port > 65535) {
                        throw std::out_of_range("Port must be between 1-65535");
                    }
                } catch (const std::exception& e) {
                    throw std::invalid_argument("Invalid port specification: " + std::string(e.what()));
                }
            }
        }
        
        // Validate that the root directory exists
        if (!std::filesystem::exists(root_path)) {
            throw std::runtime_error("Root directory does not exist: " + root_path);
        }
        
        if (!std::filesystem::is_directory(root_path)) {
            throw std::runtime_error("Specified path is not a directory: " + root_path);
        }
        
        // Create and configure the server
        HTTP_Server server(port);
        std::cout << "Starting HTTP server on port " << port << " with root directory: " << root_path << std::endl;
        
        // Register routes
        server.route("/", [](const HTTP_Request &request) {
            return HTTP_Response {
                (int)HTTP_STATUS_CODE::OK,
                "OK",
                {},
                "",
            };
        });

        server.route("/echo/", [&](const HTTP_Request &request) {
            try {
                std::cout << "Client requested echo\n";

                std::string msg = request.path.substr(6);
                
                if(request.encoding_scheme == "gzip") {
                    // Compress the message using gzip
                    std::string compressed_msg = compress_gzip(msg);

                    return HTTP_Response {
                        (int)HTTP_STATUS_CODE::OK,
                        "OK",
                        {{
                            "Content-Type", "text/plain"
                        },
                        {
                            "Content-Encoding", request.encoding_scheme
                        },
                        {
                            "Content-Length", std::to_string(compressed_msg.size())
                        }},
                        compressed_msg
                    };
                }
                
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
            } catch (const std::exception& e) {
                std::cerr << "Error in echo handler: " << e.what() << std::endl;
                return HTTP_Response {
                    (int)HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
                    "Internal Server Error",
                    {},
                    "An error occurred processing your request"
                };
            }
        });

        server.route("/files/", [&](const HTTP_Request &request){
            std::string name = request.path.substr(7);
            try {
                if(!std::filesystem::path(name).has_filename()) {
                    throw std::invalid_argument("Invalid file path");
                }
                
                // Use our path validation method to prevent directory traversal
                std::string validated_path = server.validate_file_path(root_path, name);

                if(request.method == "GET") {
                    if(auto content = server.read_file_interface(validated_path)) {
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
                } else if(request.method == "POST") {
                    std::string content = request.body;

                    server.save_file(validated_path, content);
                    return HTTP_Response {
                        (int)HTTP_STATUS_CODE::CREATED,
                        "Created",
                        {},
                        "",
                    };
                }
            } catch(const std::runtime_error &e) {
                std::string error_message = e.what();
                // Check if this was a directory traversal attempt
                if(error_message.find("traversal") != std::string::npos) {
                    return HTTP_Response {
                        (int)HTTP_STATUS_CODE::FORBIDDEN,
                        "Forbidden",
                        {},
                        "Access denied: Directory traversal attempt detected",
                    };
                }
                return HTTP_Response {
                    (int)HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    error_message,
                };
            } catch(const std::exception &e) {
                return HTTP_Response {
                    (int)HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    e.what(),
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
            try {
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
                    "User-Agent header not provided"
                };
            } catch (const std::exception& e) {
                std::cerr << "Error in user-agent handler: " << e.what() << std::endl;
                return HTTP_Response {
                    (int)HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
                    "Internal Server Error",
                    {},
                    "An error occurred processing your request"
                };
            }
        });

        // Run the server
        server.run();
    } catch(const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
