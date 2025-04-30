#ifndef SERVER_HPP
#define SERVER_HPP

#include <http_server/request.hpp> // HTTP_Request
#include <http_server/response.hpp> // HTTP_Response
#include <http_server/status.hpp>   // HTTP_STATUS_CODE
#include <http_server/config.hpp>  // HTTP_SERVER_CONFIG
#include <http_server/router.hpp>  // Router
#include <iostream>         // std::cout, std::cerr
#include <string>           // std::string
#include <map>              // std::map
#include <functional>       // std::function
#include <optional>         // std::optional
#include <arpa/inet.h>      // sockaddr_in, htons(), INADDR_ANY
#include <stdexcept>        // std::runtime_error

namespace http_server {
    class HTTP_Server {
    public:
        explicit HTTP_Server(uint16_t port = config::DEFAULT_PORT, std::string root_path = config::DEFAULT_ROOT_PATH);
        void add_route(const std::string &method, const std::string &path_pattern, Handler handler);
        void run();
        ~HTTP_Server();
    private:
        int server_fd;
        std::string root_path;
        struct sockaddr_in server_address;
        Router router;
        std::vector <std::pair<std::string, Handler>> routes;
        
        // New method to handle client connections with better error handling
        void handle_client_connection(int client_fd, const sockaddr_in& client_address);
    };
}
#endif