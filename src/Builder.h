#pragma once
#include <future>
#include <atomic>
#include "Utils.h"
#include "Data.h"
#include "Logger.h"
#include "LasPointReader.h"
#include "RawPointReader.h"
#include "PointReader.h"
#include "ThreadPool.h"

class Builder {
private:
	std::vector<std::future<void>> futures;

	Cube bounding_cube;
	uint64_t num_points;
	std::vector<std::string> las_input_paths;
	std::string output_path;
	uint32_t max_node_size;
	uint32_t sampled_node_size;

	std::atomic<uint64_t> points_processed;
	std::atomic<uint64_t> num_points_in_core;

	ThreadPool pool;

	std::mutex octree_file_lock;
	uint64_t octree_file_cursor;
	FILE* octree_file;
	std::atomic<uint32_t> open_octree_files;

	std::string octree_file_path;

	uint8_t find_child_node_index(Cube& bounds, Point& p);
	Node* create_child_node(std::string id, uint64_t num_points, std::vector<Point> points,
		float center_x, float center_y, float center_z, float size);

	uint64_t ic_sample_node(Node* node);
	void ic_load_points(Node* node);
	void ic_split_node(Node* node, bool is_async);

	void split_node(Node* node, bool is_async);
	void split_node(Node* node, bool is_async, bool is_las, std::vector<std::string> las_input_files);

	void write_node(Node* node, bool in_core);
	
public:
	Node* build();
	Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
		uint32_t max_node_size, uint32_t sampled_node_size, std::vector<std::string> las_input_paths);
};