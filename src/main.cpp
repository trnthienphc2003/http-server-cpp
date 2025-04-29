#include "server.hpp"

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
        if(request.encoding_scheme != "") {
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
                    "Content-Length", std::to_string(msg.size())
                }},
                msg
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

    server.run();
    return 0;
}
  