#include "AsyncOctreeWriter.h"

void AsyncOctreeWriter::add(Node* node, bool in_core) {
	to_write_public_lock.lock();
	to_write_public.push_back(std::make_pair(node, in_core));
	to_write_public_lock.unlock();
}

void AsyncOctreeWriter::write() {
	FILE* octree_file = fopen(get_octree_file(output_path).c_str(), "wb");
	if (!octree_file) throw std::runtime_error("Could not open file");
	uint64_t cursor = 0;
	while (!_done) {
		to_write_public_lock.lock();
		// to_write is always empty at this point
		to_write.swap(to_write_public);
		to_write_public_lock.unlock();
		for (int i = 0; i < to_write.size(); i++) {
			to_write[i].first->byte_index = cursor;
			cursor += to_write[i].first->num_points * sizeof(struct Point);
			if (to_write[i].second) {
				// In core
				fwrite(&to_write[i].first->points[0], sizeof(struct Point),
					to_write[i].first->points.size(), octree_file);
				// Remove points
				to_write[i].first->free_points();
			}
			else {
				FILE* node_points_file = fopen(get_full_point_file(to_write[i].first->id, output_path).c_str(), "rb");
				if (!node_points_file) throw std::runtime_error("Could not open file");

				for (uint64_t y = 0; y < to_write[i].first->num_points; y++) {
					Point p;
					fread(&p, sizeof(struct Point), 1, node_points_file);
					fwrite(&p, sizeof(struct Point), 1, octree_file);
				}
				fclose(node_points_file);
			}
		}
		std::vector<std::pair<Node*, bool>>().swap(to_write);
	}
}

void AsyncOctreeWriter::start(const std::string& output_path) {
	this->output_path = output_path;
	to_write.reserve(20); // reserve a little bit
	std::thread write_thread([this] {
		write();
	});
	write_thread.detach();
}

void AsyncOctreeWriter::done() {
	_done = true;
}