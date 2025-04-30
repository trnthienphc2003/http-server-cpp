#include <utils/path_validation.hpp>

#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace http_server::path_validation {
    bool is_path_inside_directory(const std::filesystem::path& path, const std::filesystem::path& directory) {
        // Get canonical absolute paths
        std::filesystem::path abs_directory = std::filesystem::canonical(directory);
        std::filesystem::path abs_path;
        
        try {
            abs_path = std::filesystem::canonical(path);
        } catch(const std::filesystem::filesystem_error&) {
            // If the path doesn't exist, check if its parent directory is valid and inside the root
            auto parent_path = path.parent_path();
            if (!std::filesystem::exists(parent_path)) {
                return false;
            }
            
            auto canonical_parent = std::filesystem::canonical(parent_path);
            // Check if the parent directory is within the root directory
            for (auto it1 = canonical_parent.begin(), it2 = abs_directory.begin();
                 it1 != canonical_parent.end() && it2 != abs_directory.end();
                 ++it1, ++it2) {
                if (*it1 != *it2) {
                    return false;
                }
            }
            return true;
        }
        
        // Compare paths to check if abs_path starts with abs_directory
        auto it1 = abs_path.begin();
        auto it2 = abs_directory.begin();
        
        while (it2 != abs_directory.end()) {
            if (it1 == abs_path.end() || *it1 != *it2) {
                return false;
            }
            ++it1;
            ++it2;
        }
        
        return true;
    }

    std::string validate_file_path(const std::string& directory_root, const std::string& requested_path) {
        try {
            // Create the full path by appending the requested path to the root directory
            std::filesystem::path root_path = directory_root;
            std::filesystem::path rel_path = requested_path;
            std::filesystem::path full_path = root_path / rel_path;
            
            // Validate that the path is inside the root directory
            if (!is_path_inside_directory(full_path, root_path)) {
                throw std::runtime_error("Directory traversal attempt detected");
            }
            
            return full_path.string();
        } catch (const std::exception& e) {
            std::cerr << "Path validation error: " << e.what() << std::endl;
            throw; // Rethrow to be handled by the caller
        }
    }
}