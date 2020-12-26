#pragma once
class Builder
{
private:
	std::string output_path;
	std::string hierarchy_prefix;
	uint32_t max_node_size;
	uint32_t sampled_node_size;
	Cube bounding_cube;
	uint64_t num_points;
	uint64_t total_final_points;

	std::mutex threads_finished_lock;
	std::vector<bool> threads_finished;

	void split_node(Node* node, bool async);
	void sample_tree(Node* node);

	void split_node_in_core(Node* node);
	void load_points(Node* node);
public:
	// Sample the children of this node into the node
	static uint64_t sample(Node* node, uint32_t num_points, std::string output_path);

	Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
		std::string hierarchy_prefix, uint32_t max_node_size, uint32_t sampled_node_size);
	Node* build();
};