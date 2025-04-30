#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <cstdint>

constexpr int BUF_LEN = 1024;
constexpr int GZIP_BUF_LEN = 32768;

namespace http_server::config {
    inline constexpr int BUF_LEN                    = 1024;
    inline constexpr uint16_t DEFAULT_PORT          = 4221;
    inline constexpr int CONNECTION_TIMEOUT         = 30; // seconds
    inline constexpr int BACKLOG_SIZE               = 10;
    inline constexpr int MAX_KEEP_ALIVE_REQUESTS    = 100;
    inline constexpr char DEFAULT_ROOT_PATH[]       = ".";
}

#endif