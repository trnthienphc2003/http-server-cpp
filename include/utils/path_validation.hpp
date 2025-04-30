#ifndef PATH_VALIDATION_HPP
#define PATH_VALIDATION_HPP

#include <filesystem>
namespace http_server::path_validation {
    // Function to validate if a path is inside a given directory
    bool is_path_inside_directory(const std::filesystem::path& path, const std::filesystem::path& directory);
    std::string validate_file_path(const std::string& directory_root, const std::string& requested_path);
}

#endif