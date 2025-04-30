#include <http_server/response.hpp>
#include <http_server/status.hpp>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string http_server::HTTP_Response::to_string() const {
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