#include <utils/file_utils.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace http_server::file_utils {
    std::optional<std::string> read_file(const std::string& file_path) {
        try {
            // Check if file exists
            if (!std::filesystem::exists(file_path)) {
                return std::nullopt;
            }
            
            std::ifstream file{file_path, std::ios::binary};
            if(!file) {
                throw std::runtime_error("Failed to open file for reading: " + file_path);
            }
    
            return {
                std::string {
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                }
            };
        } catch (const std::exception& e) {
            std::cerr << "Error reading file: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    void save_file(const std::string& file_path, const std::string& content) {
        try {
            // Validate the file path
            std::filesystem::path validated_path = file_path;
            std::filesystem::path parent_path = validated_path.parent_path();
            
            // Create directories if they don't exist
            if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
                std::filesystem::create_directories(parent_path);
            }
            
            // Write to file
            std::ofstream file(file_path, std::ios::binary);
            if(!file) {
                throw std::runtime_error("Failed to open file for writing: " + file_path);
            }
            file.write(content.data(), content.size());
            if(!file) {
                throw std::runtime_error("Failed to write to file: " + file_path);
            }
            file.close();
        } catch (const std::exception& e) {
            std::cerr << "Error saving file: " << e.what() << std::endl;
            throw; // Rethrow to be handled by the caller
        }
    }

    bool delete_file(const std::string& file_path) {
        try {
            if (std::filesystem::remove(file_path)) {
                return true;
            } else {
                std::cerr << "File not found: " << file_path << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error deleting file: " << e.what() << std::endl;
            return false;
        }
    }
}