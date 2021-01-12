#pragma once
#include <vector>
#include <string>

#define POINT_FILE_FORMAT_LAS 0
#define POINT_FILE_FORMAT_RAW 1
#define POINT_FILE_FORMAT_PTS 2

struct Point {
	float x;
	float y;
	float z;
};

struct Bounds {
	float min_x = std::numeric_limits<float>::max();
	float min_y = std::numeric_limits<float>::max();
	float min_z = std::numeric_limits<float>::max();
	float max_x = std::numeric_limits<float>::lowest();
	float max_y = std::numeric_limits<float>::lowest();
	float max_z = std::numeric_limits<float>::lowest();
};

struct Cube {
	float center_x, center_y, center_z;
	// Size is half the length of one edge
	float size;
	
	std::string to_string() {
		return "(" + std::to_string(center_x) + ", " + std::to_string(center_y) + ", " + std::to_string(center_z) + "), " + std::to_string(size);
	}
};

struct Node {
	Cube bounds;
	std::string id;
	uint64_t num_points;
	std::vector<Point> points; // Only used when splitting points in-core
	// Bit mask, the rightmost bit is the first node, the leftmost corresponds to the eighth child node
	uint64_t byte_index;
	uint8_t child_nodes_mask;
	uint8_t num_child_nodes;
	// Indices of the nodes:
	// (7) 00000111: right, top, back
	// (5) 00000101: right, bottom, back
	// (3) 00000010: left, top, front
	// (0) 00000000: left, bottom, front
	// when looking along the z axis
	Node** child_nodes;

	void free_points() {
		std::vector<Point>().swap(points);
	}
};

struct SplitPointsMetadata {
	uint64_t num_points[8] = { 0 };
	std::vector<Cube> bounding_cubes;
	std::vector<std::string> point_file_paths;
};