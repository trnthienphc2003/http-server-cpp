#include <http_server/server.hpp>
#include <http_server/compression/registry.hpp>
#include <http_server/compression/gzip.hpp>
#include <utils/file_utils.hpp>
#include <utils/path_validation.hpp>
#include <stdexcept>
#include <filesystem>

int main(int argc, char **argv) {
    try {
        // Parse command line arguments with better error handling
        uint16_t port = http_server::config::DEFAULT_PORT; // Default port
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

        // Compressors registration
        http_server::compression::CompressionRegistry::register_compressor(std::make_unique<http_server::compression::GzipCompressor>());

        // Create and configure the server
        http_server::HTTP_Server server(port, root_path);
        std::cout << "Starting HTTP server on port " << port << " with root directory: " << root_path << std::endl;
        
        // Register routes
        server.add_route("GET", "/", [&](const http_server::HTTP_Request &request, const http_server::Params &params) {
            return http_server::HTTP_Response {
                (int)http_server::HTTP_STATUS_CODE::OK,
                "OK",
                {},
                ""
            };
        });

        server.add_route("GET", "/echo/:msg", [&](const http_server::HTTP_Request &request, const http_server::Params &params) {
            try {
                std::cout << "Client requested echo\n";

                std::string msg = request.path.substr(6);
                
                auto *c = http_server::compression::CompressionRegistry::select_compressor(request.encoding_scheme);
                if(c) {
                    // Compress the message using gzip
                    if(auto compressed_msg = c->compress(msg)) {
                        return http_server::HTTP_Response {
                            (int)http_server::HTTP_STATUS_CODE::OK,
                            "OK",
                            {{
                                "Content-Type", "text/plain"
                            },
                            {
                                "Content-Encoding", request.encoding_scheme
                            },
                            {
                                "Content-Length", std::to_string((*compressed_msg).size())
                            }},
                            *compressed_msg
                        };
                    }
                }
                
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::OK,
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
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
                    "Internal Server Error",
                    {},
                    "An error occurred processing your request"
                };
            }
        });

        server.add_route("GET", "/files/:name", [&](const http_server::HTTP_Request &request, const http_server::Params &params) {
            try {
                std::string name = params.at("name");
                if(!std::filesystem::path(name).has_filename()) {
                    throw std::invalid_argument("Invalid file path");
                }
                
                // Use our path validation method to prevent directory traversal
                std::string validated_path = http_server::path_validation::validate_file_path(root_path, name);

                if(auto content = http_server::file_utils::read_file(validated_path)) {
                    return http_server::HTTP_Response {
                        (int)http_server::HTTP_STATUS_CODE::OK,
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
            } catch(const std::runtime_error &e) {
                std::string error_message = e.what();
                // Check if this was a directory traversal attempt
                if(error_message.find("traversal") != std::string::npos) {
                    return http_server::HTTP_Response {
                        (int)http_server::HTTP_STATUS_CODE::FORBIDDEN,
                        "Forbidden",
                        {},
                        "Access denied: Directory traversal attempt detected",
                    };
                }
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    error_message,
                };
            } catch(const std::exception &e) {
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    e.what(),
                };
            }

            return http_server::HTTP_Response {
                (int)http_server::HTTP_STATUS_CODE::NOT_FOUND,
                "Not Found",
                {},
                "",
            };
        });

        server.add_route("POST", "/files/:name", [&](const http_server::HTTP_Request &request, const http_server::Params &params) {
            try {
                std::string name = params.at("name");
                if(!std::filesystem::path(name).has_filename()) {
                    throw std::invalid_argument("Invalid file path");
                }
                
                // Use our path validation method to prevent directory traversal
                std::string validated_path = http_server::path_validation::validate_file_path(root_path, name);

                std::string content = request.body;

                http_server::file_utils::save_file(validated_path, content);
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::CREATED,
                    "Created",
                    {},
                    "",
                };
            } catch(const std::runtime_error &e) {
                std::string error_message = e.what();
                // Check if this was a directory traversal attempt
                if(error_message.find("traversal") != std::string::npos) {
                    return http_server::HTTP_Response {
                        (int)http_server::HTTP_STATUS_CODE::FORBIDDEN,
                        "Forbidden",
                        {},
                        "Access denied: Directory traversal attempt detected",
                    };
                }
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    error_message,
                };
            } catch(const std::exception &e) {
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    e.what(),
                };
            }

            return http_server::HTTP_Response {
                (int)http_server::HTTP_STATUS_CODE::NOT_FOUND,
                "Not Found",
                {},
                "",
            };
        });

        server.add_route("GET", "/user-agent", [&](const http_server::HTTP_Request &request, const http_server::Params &params) {
            try {
                auto it = request.headers.find("User-Agent");
                if(it != request.headers.end()) {
                    return http_server::HTTP_Response {
                        (int)http_server::HTTP_STATUS_CODE::OK,
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

                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::BAD_REQUEST,
                    "Bad Request",
                    {},
                    "User-Agent header not provided"
                };
            } catch (const std::exception& e) {
                std::cerr << "Error in user-agent handler: " << e.what() << std::endl;
                return http_server::HTTP_Response {
                    (int)http_server::HTTP_STATUS_CODE::INTERNAL_SERVER_ERROR,
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