#include "server.hpp"
#include <stdexcept>
#include <zlib.h>

static std::string compress_gzip(std::string &data) {
    /*
        Encode the data using gzip
        This function should handle gzip compression
    */

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
            throw std::runtime_error("deflate failed");
        }

        std::size_t have = sizeof(buffer) - zstream.avail_out;
        out.append(buffer, have);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zstream);
    return out;
}

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

    try {
        server.run();
    } catch(const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
  