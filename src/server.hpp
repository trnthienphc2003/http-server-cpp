#ifndef SERVER_HPP
#define SERVER_HPP
#include <iostream>         // std::cout, std::cerr
#include <string>           // std::string
#include <map>              // std::map
#include <functional>       // std::function
#include <optional>         // std::optional
#include <arpa/inet.h>      // sockaddr_in, htons(), INADDR_ANY

constexpr int BUF_LEN = 1024;
constexpr int GZIP_BUF_LEN = 32768;
enum class HTTP_STATUS_CODE{
    OK = 200,
    CREATED = 201,
    BAD_REQUEST = 400,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    INTERNAL_SERVER_ERROR = 500,
};
struct HTTP_Request {
    std::string method, path, version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string encoding_scheme;
};

struct HTTP_Response {
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string to_string() const;
};

class HTTP_Server {
public:
    using Handler = std::function<HTTP_Response(const HTTP_Request &)>;
    HTTP_Server(uint16_t port);
    ~HTTP_Server();
    void run();
    void route(const std::string &path_pattern, Handler handler);
    void save_file(const std::string &file_path, const std::string &content);
    std::optional<std::string> read_file_interface(const std::string& file_path);
private:
    int server_fd;
    struct sockaddr_in server_address;
    std::vector <std::pair<std::string, Handler>> routes;
    HTTP_Request parse_request(const std::string &raw);
    HTTP_Response dispatch(const HTTP_Request &request);
    std::optional<std::string> read_file(const std::string &file_path);
};
    

#endif