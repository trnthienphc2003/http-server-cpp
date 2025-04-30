#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <http_server/request.hpp>  // HTTP_Request
#include <http_server/response.hpp> // HTTP_Response
#include <http_server/status.hpp>   // HTTP_STATUS_CODE
#include <functional>               // std::function
#include <regex>                    // std::regex

namespace http_server {
    using Params = std::map<std::string, std::string>;
    using Handler = std::function<HTTP_Response(const HTTP_Request &, const Params &)>;

    struct Route {
        std::string method;
        std::string path_pattern;
        std::regex regex_pattern;
        std::vector<std::string> path_params;
        Handler handler;
    };

    class Router {
        private:
            std::vector<Route> routes;
        public:
            HTTP_Response dispatch(const HTTP_Request &request) const;
            void add_route(const std::string &method, const std::string &path_pattern, Handler handler);
            static std::pair<std::regex, std::vector<std::string>> compile_path_pattern(const std::string &path_pattern);
    };
}

#endif