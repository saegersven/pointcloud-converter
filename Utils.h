#pragma once
#include <filesystem>
#include <string>

bool is_directory_empty(const std::string& path);

bool check_file(const std::string& filename);

std::string get_directory(std::string file);

std::string get_full_point_file(std::string hierarchy, std::string output_path, std::string hierarchy_prefix);