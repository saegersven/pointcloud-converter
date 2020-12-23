#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include "Data.h"

class Reader
{
private:
	std::string input_path;
	std::string output_path;
	uint32_t chunk_size;
public:
	std::string temp_point_path;
	Reader(std::string input_path, std::string output_path);
	Cube read_bounds();
	SplitPointsMetadata split_points(Cube bounding_cube, std::string point_file_path);
};