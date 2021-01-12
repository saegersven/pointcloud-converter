#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "Data.h"
#include "Utils.h"

class AsyncOctreeWriter
{
private:
	std::string output_path;

	std::mutex to_write_public_lock;
	std::vector<std::pair<Node*, bool>> to_write;
	std::vector<std::pair<Node*, bool>> to_write_public;

	std::atomic<bool> _done;

	void write();

public:
	void add(Node* node, bool in_core);
	void start(const std::string& output_path);
	void done();
};

