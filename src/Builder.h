#pragma once
#include <future>
#include <atomic>
#include "Utils.h"
#include "Data.h"
#include "Logger.h"

class Builder {
private:
	std::vector<std::future<void>> futures;

	Cube bounding_cube;
	uint64_t num_points;
	std::string output_path;
	uint32_t max_node_size;
	uint32_t sampled_node_size;

	std::atomic<uint64_t> points_processed;
	std::atomic<uint64_t> num_points_in_core;

	uint8_t find_child_node_index(Cube& bounds, Point& p);
	Node* create_child_node(std::string id, uint64_t num_points, std::vector<Point> points,
		float center_x, float center_y, float center_z, float size);

	void ic_sample_node(Node* node);
	void ic_load_points(Node* node);
	void ic_split_node(Node* node);

	void split_node(Node* node, bool is_async);

public:
	Node* build();
	Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
		uint32_t max_node_size, uint32_t sampled_node_size);
};