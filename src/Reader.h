#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <iostream>
#include <chrono>
#include "Data.h"
#include "BufferedPointReader.h"
#include "BufferedPointWriter.h"
#include "Logger.h"

class Reader
{
private:
	std::vector<std::string> input_path;
	std::string output_path;
	uint32_t chunk_size;
public:
	std::string temp_point_path;
	Reader(std::vector<std::string> input_path, std::string output_path);
	Cube read_bounds();
	SplitPointsMetadata split_points(Cube bounding_cube);
	static void read_bounds_las(std::vector<std::string> input_files, Cube& bounds, uint64_t& num_points);
};