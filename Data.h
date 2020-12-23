#pragma once
#include <vector>

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
};

struct Node {
	Cube bounds;
	std::string id;
	uint64_t num_points;
	// Bit mask, the rightmost bit is the first node, the leftmost corresponds to the eighth child node
	uint8_t child_nodes_mask;
	// Indices of the nodes:
	// (7) 00000111: right, top, back
	// (5) 00000101: right, bottom, back
	// (3) 00000010: left, top, front
	// (0) 00000000: left, bottom, front
	// when looking along the z axis
	Node** child_nodes;
};

struct SplitPointsMetadata {
	uint64_t num_points[8] = { 0 };
	std::vector<Cube> bounding_cubes;
	std::vector<std::string> point_file_paths;
};