#include <http_server/request.hpp>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <algorithm>       // std::remove

http_server::HTTP_Request http_server::parse_request(const std::string &raw) {
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