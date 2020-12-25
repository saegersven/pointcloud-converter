#include "BufferedPointWriter.h"

BufferedPointWriter::BufferedPointWriter(std::string output_path, uint64_t write_threshold) : to_write() {
	this->output_path = output_path;
	this->total_points = 0;
	this->write_threshold = write_threshold;
}

void BufferedPointWriter::write() {
	while (true) {
		status_lock.lock();
		if (_done && total_points == 0) {
			status_lock.unlock();
			break;
		}
		if (!_done && total_points < write_threshold) {
			status_lock.unlock();
			continue; // If we are not done and there are not enough points, wait
		}
		status_lock.unlock();
		to_write_lock.lock();
		std::map<std::string, std::vector<Point>>::iterator it = to_write.begin();

		while (it != to_write.end()) {
			FILE* f;
#ifdef _BUILD_CMAKE
			f = fopen(get_full_point_file(it->first, output_path, ""));
			bool file_open = f;
#else
			bool file_open = fopen_s(&f, get_full_point_file(it->first, output_path, "").c_str(), "ab") == NULL && f;
#endif
			if(!file_open)
				throw std::exception("Could not open file");

			fwrite(&it->second[0], sizeof(struct Point), it->second.size(), f); // Write points

			fclose(f);

			total_points = 0;
			it++;
		}
		to_write.clear();
		to_write_lock.unlock();
	}
	status_lock.lock();
	_finished = true;
	status_lock.unlock();
}

void BufferedPointWriter::start_writing() {
	std::thread writer_thread([this] {
		write();
	});
	writer_thread.detach();
}

void BufferedPointWriter::done() {
	status_lock.lock();
	_done = true;
	status_lock.unlock();
}

void BufferedPointWriter::schedule_points(std::string hierarchy, std::vector<Point>& points) {
	to_write_lock.lock();
	if (to_write.find(hierarchy) != to_write.end()) {
		// Append points
		uint64_t size = to_write[hierarchy].size();
		to_write[hierarchy].resize(size + points.size());
		for (uint64_t i = 0; i < points.size(); i++) {
			to_write[hierarchy][size + i] = points[i];
		}
	}
	else {
		std::vector<Point> points_copy = points;
		to_write.insert(std::pair<std::string, std::vector<Point>>(hierarchy, points_copy));
	}
	to_write_lock.unlock();

	total_points += points.size();
}

bool BufferedPointWriter::finished() {
	status_lock.lock();
	bool b = _finished;
	status_lock.unlock();
	return b;
}