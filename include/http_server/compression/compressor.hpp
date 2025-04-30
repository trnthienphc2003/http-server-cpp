#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <string>
#include <optional>

namespace http_server::compression {
    inline constexpr int GZIP_BUF_LEN = 32768; // Buffer length for GZIP compression
    class Compressor {
    public:
        virtual ~Compressor() = default;
        virtual std::optional<std::string> compress(const std::string &data) = 0;
        virtual std::string encoding_name() const = 0;
    };
}

#endif