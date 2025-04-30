#include <http_server/compression/gzip.hpp>
#include <zlib.h>
#include <stdexcept>
#include <iostream>

namespace http_server::compression {
    std::optional<std::string> GzipCompressor::compress(const std::string &data) {
        try {
            z_stream zstream{};
            if(deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
                throw std::runtime_error("Failed to initialize zlib");
            }
    
            zstream.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
            zstream.avail_in  = data.size();
    
            std::string out;
            out.reserve(data.size() / 2);  // heuristic
    
            char buffer[GZIP_BUF_LEN];
            int ret;
            do {
                zstream.next_out  = reinterpret_cast<Bytef*>(buffer);
                zstream.avail_out = sizeof(buffer);
    
                ret = deflate(&zstream, Z_FINISH);
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    deflateEnd(&zstream);
                    throw std::runtime_error("Deflate failed: " + std::string(zstream.msg ? zstream.msg : "unknown error"));
                }
    
                std::size_t have = sizeof(buffer) - zstream.avail_out;
                out.append(buffer, have);
            } while (ret != Z_STREAM_END);
    
            deflateEnd(&zstream);
            return out;
        } catch (const std::exception& e) {
            std::cerr << "Compression error: " << e.what() << std::endl;
            // In case of compression failure, return the original data
            // This is safer than returning an empty string
            return data;
        }
    }
}
