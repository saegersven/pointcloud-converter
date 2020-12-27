#pragma once
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include "Utils.h"
#include "Data.h"

class BufferedPointWriter {
private:
	bool _done;
	bool _finished;
	std::mutex status_lock;

	std::string output_path;

	std::map<std::string, std::vector<Point>> to_write;
	std::mutex to_write_lock;

	uint64_t total_points;
	uint64_t write_threshold;
	void write();
public:
	BufferedPointWriter(std::string output_path, uint64_t write_threshold);
	void start_writing();
	void done();
	void schedule_points(std::string hierarchy, std::vector<Point>& points);

	bool finished();
};