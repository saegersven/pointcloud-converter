#pragma once
#include <string>
#include <iostream>
#include <random>
#include "Utils.h"
#include "Data.h"

class Builder
{
private:
	std::string output_path;
	std::string hierarchy_prefix;
	uint32_t max_node_size;
	uint32_t sampled_node_size;
	Cube bounding_cube;
	uint64_t num_points;

	void split_node(Node* node);
	void sample_tree(Node* node);
public:
	// Sample the children of this node into the node
	static uint64_t sample(Node* node, uint32_t num_points, std::string output_path);

	Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
		std::string hierarchy_prefix, uint32_t max_node_size, uint32_t sampled_node_size);
	Node* build();
};