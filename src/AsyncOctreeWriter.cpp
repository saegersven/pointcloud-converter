#include "AsyncOctreeWriter.h"
#include "Logger.h"
#include "ThreadPool.h"

void AsyncOctreeWriter::add(Node* node, bool in_core) {
	/*to_write_lock.lock();
	to_write.push_back(std::make_pair(node, in_core));
	to_write_lock.unlock();*/
	pool.add_job([this, node, in_core] {
		node->byte_index = byte_cursor; // Set the byte index so we know where the points are in the file
		byte_cursor += node->points.size() * sizeof(struct Point); // Increment byte cursor by what is going to be added to the file
		if (in_core) {
			// In core
			octree_write_lock.lock();
			fwrite(&node->points[0], sizeof(struct Point),
				node->points.size(), octree_file);
			octree_write_lock.unlock();
			// Remove points
			num_points_in_core -= node->points.size();
			std::vector<Point>().swap(node->points);
		}
		else {
			std::string path = get_full_point_file(node->id, output_path);
			FILE* node_points_file = fopen(path.c_str(), "rb");
			if (!node_points_file) THROW_FILE_OPEN_ERROR;

			octree_write_lock.lock();
			for (uint64_t y = 0; y < node->num_points; y++) {
				Point p;
				fread(&p, sizeof(struct Point), 1, node_points_file);
				fwrite(&p, sizeof(struct Point), 1, octree_file);
			}
			octree_write_lock.unlock();
			fclose(node_points_file);

			// Delete the file
			remove(path.c_str());
		}
	});
}

void AsyncOctreeWriter::start(const std::string& output_path) {
	this->output_path = output_path;
	this->num_points_in_core = 0;

	octree_file = fopen(get_octree_file(output_path).c_str(), "wb");
	if (!octree_file) THROW_FILE_OPEN_ERROR;
	bool active = true;
}

void AsyncOctreeWriter::done() {
	_done = true;
	Logger::log_info("Waiting for writer...");
	pool.wait();
}

uint64_t AsyncOctreeWriter::get_num_points_in_core() {
	return num_points_in_core.load();
}

void AsyncOctreeWriter::add_num_points_in_core(uint64_t points) {
	num_points_in_core += points;
}

AsyncOctreeWriter::AsyncOctreeWriter(const uint16_t num_threads) : pool(num_threads) {}