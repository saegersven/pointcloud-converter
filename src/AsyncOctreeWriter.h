#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "Data.h"
#include "Utils.h"
#include "ThreadPool.h"

class AsyncOctreeWriter
{
private:
	std::string output_path;

	//std::mutex to_write_lock;
	//std::vector<std::pair<Node*, bool>> to_write;
	ThreadPool pool;
	FILE* octree_file;
	std::mutex octree_write_lock;
	uint64_t byte_cursor; // Gets incremented when a node gets added

	std::atomic<bool> _done;

	std::atomic<uint64_t> num_points_in_core;

public:
	void add(Node* node, bool in_core); // Returns byte index
	void start(const std::string& output_path);
	void done();

	uint64_t get_num_points_in_core();
	void add_num_points_in_core(uint64_t points);

	AsyncOctreeWriter(const uint16_t num_threads);
};

