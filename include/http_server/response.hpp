#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>

namespace http_server {
    struct HTTP_Response {
        int status_code;
        std::string status_message;
        std::map<std::string, std::string> headers;
        std::string body;
        
        std::string to_string() const;
    };
}
#endif