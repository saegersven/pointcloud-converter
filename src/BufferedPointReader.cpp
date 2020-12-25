#include "BufferedPointReader.h"

uint64_t BufferedPointReader::start_reading(Point*& buffer) { // Only called by main thread
	if (!points_available() && eof()) return 0;
	while (!points_available()); // Wait for points to become available
	buffer_lock.lock();

	buffer = *public_buffer;
	return points_in_buffer;
}

void BufferedPointReader::stop_reading() { // Only called by main thread
	_points_available = false;
	buffer_lock.unlock();
}

bool BufferedPointReader::points_available() {
	return _points_available;
}

bool BufferedPointReader::eof() {
	return _eof;
}

void BufferedPointReader::swap_buffers(uint64_t num_points) { // Only called by async thread
	buffer_lock.lock();
	// Swap buffers
	private_buffer = &(_public_buffer ? buffer1 : buffer0);
	public_buffer = &(_public_buffer ? buffer0 : buffer1);
	_points_available = true;
	points_in_buffer = num_points;
	buffer_lock.unlock();
}

bool BufferedPointReader::read_point_raw(Point& p) {
	return fread(&p, sizeof(p), 1, bin_file);
}

bool BufferedPointReader::open_file_raw() {
	bin_file = fopen(file.c_str(), "rb");
	return bin_file;
}

void BufferedPointReader::read() { // Only called by async thread
	bool open_success = false;

	switch (format) {
	case POINT_FILE_FORMAT_RAW:
		open_success = open_file_raw();
		break;
	}

	if (!open_success) throw std::exception("Could not open file");

	uint64_t i = 0;
	bool eof = false;
	while (!eof) {
		Point p;

		switch (format) {
		case POINT_FILE_FORMAT_RAW:
			eof = !read_point_raw(p);
			break;
		}

		if (eof) break;

		(*private_buffer)[i] = p;
		i++;

		if (i == buffer_size) {
			i = 0;
			swap_buffers(buffer_size);
		}
	}
	if (i != 0) swap_buffers(i);
}

void BufferedPointReader::start() {
	std::thread read_thread([this] {
		read();
		while (points_available()); // Wait for all points to be read
		_eof = true;
		buffer_lock.lock(); // Wait for mutex to be unlocked to safely terminate the thread
		buffer_lock.unlock();

		delete[] buffer0;
		delete[] buffer1;
	});
	read_thread.detach();
}

BufferedPointReader::BufferedPointReader(std::string file, uint8_t format, uint64_t buffer_size) {
	this->file = file;
	this->format = format;
	this->buffer_size = buffer_size;

	buffer0 = new Point[buffer_size];
	buffer1 = new Point[buffer_size];

	_public_buffer = false;
	public_buffer = &buffer0;
	private_buffer = &buffer1;

	_eof = false;
	_points_available = false;

	points_in_buffer = 0;
	bin_file = nullptr;
}