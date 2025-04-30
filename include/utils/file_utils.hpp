#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>
#include <optional>
namespace http_server::file_utils {
    // Read an entire file into a string
    std::optional<std::string> read_file(const std::string& file_path);

    // Write 'data' into 'path', overwriting if exists
    void save_file(const std::string& path, const std::string& data);

    // Delete the file at 'path
    bool delete_file(const std::string& path);
}


#endif