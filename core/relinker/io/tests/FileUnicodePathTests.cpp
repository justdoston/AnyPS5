#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    try {
        std::filesystem::path name(u8"anyps5-file-path-ü日-");
        name += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto directory = std::filesystem::temp_directory_path() / name;
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Cannot create the test directory");
        const auto file = directory / std::filesystem::path(u8"données.bin");
        const std::string text = file.string();
        const std::vector<std::uint8_t> bytes = {1, 2, 3, 0xff};
        Io::FileWriter().Write(text, bytes);
        if (!std::filesystem::is_regular_file(file))
            throw std::runtime_error("FileWriter did not create the file at its UTF-8 path");
        if (Io::FileReader().Read(text) != bytes)
            throw std::runtime_error("FileReader did not read the bytes back from the UTF-8 path");
        Io::FileWriter().Write(text, std::string("text"));
        if (std::filesystem::file_size(file) != 4)
            throw std::runtime_error("FileWriter did not replace the file at its UTF-8 path");
        std::filesystem::remove_all(directory);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
