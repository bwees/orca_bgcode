#include "convert/convert.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <stdlib.h>
#include <boost/nowide/cstdio.hpp>

using namespace bgcode::core;
using namespace bgcode::binarize;
using namespace bgcode::convert;

struct Parameter
{
    std::string_view name;
    std::vector<std::string_view> values;
    size_t default_id;
};

using namespace std::literals;

static const BinarizerConfig DefaultBinarizerConfig;

static const std::vector<Parameter> parameters = {
    { "checksum"sv, { "None"sv, "CRC32"sv }, (size_t) DefaultBinarizerConfig.checksum },
    { "file_metadata_compression"sv, { "None"sv, "Deflate"sv, "Heatshrink_11_4"sv, "Heatshrink_12_4"sv }, (size_t)DefaultBinarizerConfig.compression.file_metadata },
    { "print_metadata_compression"sv, { "None"sv, "Deflate"sv, "Heatshrink_11_4"sv, "Heatshrink_12_4"sv }, (size_t)DefaultBinarizerConfig.compression.print_metadata },
    { "printer_metadata_compression"sv, { "None"sv, "Deflate"sv, "Heatshrink_11_4"sv, "Heatshrink_12_4"sv }, (size_t)DefaultBinarizerConfig.compression.printer_metadata },
    { "slicer_metadata_compression"sv, { "None"sv, "Deflate"sv, "Heatshrink_11_4"sv, "Heatshrink_12_4"sv }, (size_t)DefaultBinarizerConfig.compression.slicer_metadata },
    { "gcode_compression"sv, { "None"sv, "Deflate"sv, "Heatshrink_11_4"sv, "Heatshrink_12_4"sv }, (size_t)DefaultBinarizerConfig.compression.gcode },
    { "gcode_encoding"sv, { "None"sv, "MeatPack"sv, "MeatPackComments"sv }, (size_t)DefaultBinarizerConfig.gcode_encoding },
    { "metadata_encoding"sv, { "INI"sv }, (size_t) DefaultBinarizerConfig.metadata_encoding },
};

class ScopedFile
{
public:
    explicit ScopedFile(FILE* file) : m_file(file) {}
    ~ScopedFile() { if (m_file != nullptr) fclose(m_file); }
private:
    FILE* m_file{ nullptr };
};

void show_help() {
    std::cout << "Usage: bgcode filename [ Binarization parameters ]\n";
    std::cout << "\nBinarization parameters (used only when converting to binary format):\n";
    for (const Parameter& p : parameters) {
        std::cout << "--" << p.name << "=X\n";
        std::cout << "  where X is one of:\n";
        size_t count = 0;
        for (const std::string_view& v : p.values) {
            std::cout << "  " << count << ") " << v;
            if (count == p.default_id)
                std::cout << " (default)";
            std::cout << "\n";
            ++count;
        }
    }
}

bool parse_args(int argc, const char* argv[], std::string& src_filename, bool& src_is_binary, bool& is_post_processing, BinarizerConfig& config)
{
    if (argc < 2) {
        show_help();
        return false;
    }

    std::vector<std::string_view> arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.push_back(argv[i]);
    }

    size_t pos = arguments[argc-1].find_last_of(".");
    if (pos == std::string::npos) {
        std::cout << "Invalid filename " << arguments[argc-1] << " (required extensions: .gcode or .bgcode or .bgc)\n";
        return false;
    }

    // check if we are a post-processor
    for (size_t i = 1; i < arguments.size()-1; ++i) {
        auto a = arguments[i];
        if (a.find("post-processor") != std::string_view::npos) {
            is_post_processing = true;
            break;
        }
    }

    // only do a extension check if we are not post processing
    if (!is_post_processing) {
        const std::string_view extension = arguments[argc-1].substr(pos);
        if (extension != ".gcode" && extension != ".GCODE" &&
            extension != ".pp" && extension != ".PP" &&
            extension != ".bgcode" && extension != ".BGCODE" &&
            extension != ".bgc" && extension != ".BGC") {
            std::cout << "Found invalid extension '" << extension << "' (required extensions: .gcode or .bgcode or .bgc)\n";
            return false;
        }
    }
    src_filename = arguments[argc-1];

    FILE* src_file = boost::nowide::fopen(src_filename.c_str(), "rb");
    if (src_file == nullptr) {
        std::cout << "Unable to open file '" << src_filename << "'\n";
        return false;
    }
    src_is_binary = is_valid_binary_gcode(*src_file) == EResult::Success;
    fclose(src_file);

    if (!src_is_binary) {
        for (size_t i = 1; i < arguments.size()-1; ++i) {
            const std::string_view& a = arguments[i];
            if (a.length() < 2 || a[0] != '-' || a[1] != '-') {
                std::cout << "Found invalid parameter '" << a << "'\n";
                std::cout << "Required syntax: --parameter=value\n";
                return false;
            }

            pos = a.find("=");
            if (pos == std::string_view::npos) {
                if (a.find("post-processor") != std::string_view::npos) {
                    continue;
                }

                std::cout << "Found invalid parameter '" << a << "'\n";
                std::cout << "Required syntax: --parameter=value\n";
                return false;
            }

            const std::string_view key = a.substr(2, pos - 2);
            auto it = std::find_if(parameters.begin(), parameters.end(),
                [&key](const Parameter& item) { return item.name == key; });
            if (it == parameters.end()) {
                std::cout << "Found unknown parameter '" << key << "'\n";
                std::cout << "Accepted parameters:\n";
                for (const Parameter& p : parameters) {
                    std::cout << p.name << "\n";
                }
                return false;
            }

            const Parameter& parameter = parameters[std::distance(parameters.begin(), it)];

            const std::string_view value_str = a.substr(pos + 1);
            int value;
            try {
                value = std::stoi(std::string(value_str));
                if (value >= parameter.values.size())
                    throw std::runtime_error("invalid value");
            }
            catch (...) {
                std::cout << "Found invalid value for parameter '" << parameter.name << "'\n";
                std::cout << "Accepted values:\n";
                for (size_t p = 0; p < parameter.values.size(); ++p) {
                    std::cout << p << ") " << parameter.values[p] << "\n";
                }
                return false;
            }

            if (parameter.name == "checksum")
                config.checksum = (EChecksumType)value;
            else if (parameter.name == "file_metadata_compression")
                config.compression.file_metadata = (ECompressionType)value;
            else if (parameter.name == "print_metadata_compression")
                config.compression.print_metadata = (ECompressionType)value;
            else if (parameter.name == "printer_metadata_compression")
                config.compression.printer_metadata = (ECompressionType)value;
            else if (parameter.name == "slicer_metadata_compression")
                config.compression.slicer_metadata = (ECompressionType)value;
            else if (parameter.name == "gcode_compression")
                config.compression.gcode = (ECompressionType)value;
            else if (parameter.name == "gcode_encoding")
                config.gcode_encoding = (EGCodeEncodingType)value;
            else if (parameter.name == "metadata_encoding")
                config.metadata_encoding = (EMetadataEncodingType)value;
        }
    }
    return true;
}

int main(int argc, const char* argv[])
{
    std::string src_filename;
    std::string dst_filename;
    bool src_is_binary;
    bool is_post_processing = false;
    BinarizerConfig config;
    if (!parse_args(argc, argv, src_filename, src_is_binary, is_post_processing, config))
        return EXIT_FAILURE;


    // scope for files
    {
        // Open source file
        FILE* src_file = boost::nowide::fopen(src_filename.c_str(), "rb");
        if (src_file == nullptr) {
            std::cout << "Unable to open file '" << src_filename << "'\n";
            return EXIT_FAILURE;
        }
        ScopedFile scoped_src_file(src_file);
    
        const size_t pos = src_filename.find_last_of(".");
        const std::string src_stem = src_filename.substr(0, pos);
        const std::string src_extension = (pos != std::string::npos) ? src_filename.substr(pos) : "";
        const std::string dst_extension = src_is_binary ?
            (src_extension == ".gcode") ? ".1.gcode" : ".gcode" :
            (src_extension == ".bgcode") ? ".1.bgcode" : ".bgcode";
        dst_filename = src_stem + dst_extension;
    
        // Open destination file
        FILE* dst_file = boost::nowide::fopen(dst_filename.c_str(), "wb");
        if (dst_file == nullptr) {
            std::cout << "Unable to open file '" << dst_filename << "'\n";
            return EXIT_FAILURE;
        }
        ScopedFile scoped_dst_file(dst_file);
    
        // Perform conversion
        const EResult res = src_is_binary ? from_binary_to_ascii(*src_file, *dst_file, true) : from_ascii_to_binary(*src_file, *dst_file, config);
        if (res == EResult::Success) {
            if (!src_is_binary) {
                std::cout << "Binarization parameters\n";
                for (const Parameter& p : parameters) {
                    std::cout << p.name << ": ";
                    if (p.name == "checksum")
                        std::cout << p.values[(size_t)config.checksum] << "\n";
                    else if (p.name == "file_metadata_compression")
                        std::cout << p.values[(size_t)config.compression.file_metadata] << "\n";
                    else if (p.name == "print_metadata_compression")
                        std::cout << p.values[(size_t)config.compression.print_metadata] << "\n";
                    else if (p.name == "printer_metadata_compression")
                        std::cout << p.values[(size_t)config.compression.printer_metadata] << "\n";
                    else if (p.name == "slicer_metadata_compression")
                        std::cout << p.values[(size_t)config.compression.slicer_metadata] << "\n";
                    else if (p.name == "gcode_compression")
                        std::cout << p.values[(size_t)config.compression.gcode] << "\n";
                    else if (p.name == "gcode_encoding")
                        std::cout << p.values[(size_t)config.gcode_encoding] << "\n";
                    else if (p.name == "metadata_encoding")
                        std::cout << p.values[(size_t)config.metadata_encoding] << "\n";
                }
            }
            std::cout << "Succesfully generated file '" << dst_filename << "'\n";
        }
        else {
            std::cout << "Unable to convert the file '" << src_filename << "'\n";
            std::cout << "Error: " << translate_result(res) << "\n";
            return EXIT_FAILURE;
        }
    }

    if (is_post_processing) {

        // we have to delete the original file first before renaming the new file
        // if this line is not here, windows will not allow the rename to happen
        if (std::remove(src_filename.c_str()) != 0) {
            std::cout << "Unable to delete file '" << src_filename << "'\n";
            return EXIT_FAILURE;
        }

        // move file to be in place of the original file
        if (std::rename(dst_filename.c_str(), src_filename.c_str()) != 0) {
            std::cout << "Unable to move file '" << dst_filename << "' to '" << src_filename << "'\n";
            return EXIT_FAILURE;
        }

        // provide suggestion for new filename in file
        
        // get env SLIC3R_PP_OUTPUT_NAME
        const char* gcode_output_name_c_str = std::getenv("SLIC3R_PP_OUTPUT_NAME");
        const std::string gcode_output_name = gcode_output_name_c_str;
        const size_t gcode_pos = gcode_output_name.find_last_of(".");
        const std::string src_stem_bgcode = gcode_output_name.substr(0, gcode_pos) + ".bgcode";


        FILE* new_name = boost::nowide::fopen((src_filename + ".output_name").c_str(), "wb");
        if (new_name == nullptr) {
            std::cout << "Unable to open file '" << src_filename << ".output_name'\n";
            return EXIT_FAILURE;
        }

        std::cout << "Suggested new filename: " << src_stem_bgcode << "\n";
        std::cout << "Suggested new filename file: " << src_filename + ".output_name" << "\n";

        fwrite(src_stem_bgcode.c_str(), 1, src_stem_bgcode.size(), new_name);
        fclose(new_name);
    }

    return EXIT_SUCCESS;
}
