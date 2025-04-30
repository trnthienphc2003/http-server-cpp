#include <http_server/compression/registry.hpp>
#include <vector>

namespace http_server::compression {
    std::vector<std::unique_ptr<Compressor>> &CompressionRegistry::all() {
        static std::vector<std::unique_ptr<Compressor>> compressors;
        return compressors;
    }
    void CompressionRegistry::register_compressor(std::unique_ptr<Compressor> compressor) {
        /*
        * Register a compressor. This function takes ownership of the compressor
        * and stores it in a static vector. The vector is static to ensure that
        * the compressors are available for the lifetime of the program.
        *
        * @param compressor A unique pointer to a Compressor object.
        */
        all().push_back(std::move(compressor));
    }

    Compressor* CompressionRegistry::select_compressor(const std::string &accept_encodings) {
        /*
        * Choose a compressor based on the Accept-Encoding header.
        * This function iterates through all registered compressors and
        * returns the first one that matches the provided accept_encodings.
        * If no match is found, it returns nullptr.
        */
        for (const auto &compressor : all()) {
            if (accept_encodings.find(compressor->encoding_name()) != std::string::npos) {
                return compressor.get();
            }
        }
        return nullptr;
    }
}