#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

namespace http_server {
    struct HTTP_Request {
        std::string method, path, version;
        std::map<std::string, std::string> headers;
        std::string body;
        std::string encoding_scheme;
    };

    HTTP_Request parse_request(const std::string &raw);
}

#endif