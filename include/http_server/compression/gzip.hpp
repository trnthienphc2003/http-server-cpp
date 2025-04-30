#ifndef GZIP_HPP
#define GZIP_HPP
#include "compressor.hpp"

namespace http_server::compression {
    class GzipCompressor: public Compressor {
    public:
        std::optional<std::string> compress(const std::string &data) override;
        std::string encoding_name() const override {
            return "gzip";
        }
    };
}

#endif