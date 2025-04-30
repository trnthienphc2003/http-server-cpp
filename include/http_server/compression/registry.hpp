#ifndef REGISTRY_HPP
#define REGISTRY_HPP
#include "compressor.hpp"
#include <memory>
#include <vector>

namespace http_server::compression {
    class CompressionRegistry {
    public:
        static void register_compressor(std::unique_ptr<Compressor> compressor);
        static Compressor* select_compressor(const std::string &accept_encodings);
    private:
        // hidden helper returning the one true vector by reference
        static std::vector<std::unique_ptr<Compressor>>& all();
    };
}

#endif