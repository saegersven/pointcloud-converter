#include "BufferedPointReader.h"
#include <iostream>

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
	return _points_available.load();
}

bool BufferedPointReader::eof() {
	return _eof.load();
}

bool BufferedPointReader::done() {
	return _done.load();
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

// RAW POINT FILES
bool BufferedPointReader::read_point_raw(FILE* f, Point& p) {
	return fread(&p, sizeof(p), 1, f);
}

bool BufferedPointReader::open_file_raw(FILE*& f) {
	f = fopen(file.c_str(), "rb");
	return f;
}

// LAS POINT FILE
bool BufferedPointReader::read_point_las(FILE* f, Point& p) {
	if (las_num_points >= las_total_points) return false;

	int32_t x, y, z;
	bool c = fread(&x, sizeof(int32_t), 1, f);
	bool d = fread(&y, sizeof(int32_t), 1, f);
	bool e = fread(&z, sizeof(int32_t), 1, f);
	fseek(f, las_skip_bytes, SEEK_CUR);

	// No offset, because the coordinates will become very big with some point clouds
	p.x = x * las_scale_x + las_offset_x;
	p.y = y * las_scale_y + las_offset_y;
	p.z = z * las_scale_z + las_offset_z;
	
	p.x = x * las_scale_x;
	p.y = y * las_scale_y;
	p.z = z * las_scale_z;

	las_num_points++;

	return c && d && e;
}

bool BufferedPointReader::open_file_las(FILE*& f) {
	f = fopen(file.c_str(), "rb");
	if (!f) return false;

	// READ HEADER
	fseek(f, 96, SEEK_SET);
	fread(&las_first_point, sizeof(las_first_point), 1, f); // Read offset to first point

	fseek(f, 5, SEEK_CUR);
	fread(&las_skip_bytes, sizeof(las_skip_bytes), 1, f);
	las_skip_bytes -= 12; // Subtract size of coordinates

	fread(&las_legacy_total_points, sizeof(uint32_t), 1, f);
	if (las_legacy_total_points == 0) {
		fseek(f, 140, SEEK_SET); // Seek the new long long number of points
		fread(&las_total_points, sizeof(uint64_t), 1, f);
		fseek(f, 101, SEEK_SET); // Rewind back to the legacy number to continue
	}
	else {
		las_total_points = las_legacy_total_points;
	}

	fseek(f, 131, SEEK_SET);
	fread(&las_scale_x, sizeof(las_scale_x), 1, f);
	fread(&las_scale_y, sizeof(las_scale_y), 1, f);
	fread(&las_scale_z, sizeof(las_scale_z), 1, f);

	fread(&las_offset_x, sizeof(las_offset_x), 1, f);
	fread(&las_offset_y, sizeof(las_offset_y), 1, f);
	fread(&las_offset_z, sizeof(las_offset_z), 1, f);

	fseek(f, las_first_point, SEEK_SET); // Seek the first point to start reading
	return f;
}

void BufferedPointReader::read() { // Only called by async thread
	FILE* bin_file = nullptr;
	bool open_success = false;

	switch (format) {
	case POINT_FILE_FORMAT_RAW:
		open_success = open_file_raw(bin_file);
		break;
	case POINT_FILE_FORMAT_LAS:
		open_success = open_file_las(bin_file);
		break;
	}

	if (!open_success) throw std::runtime_error("Could not open file");

	uint64_t i = 0;
	_eof.store(false);
	while (!_eof.load()) {
		Point p;

		switch (format) {
		case POINT_FILE_FORMAT_RAW:
			_eof.store(!read_point_raw(bin_file, p));
			break;
		case POINT_FILE_FORMAT_LAS:
			_eof.store(!read_point_las(bin_file, p));
			break;
		}

		if (_eof.load()) break;

		(*private_buffer)[i] = p;
		i++;

		if (i == buffer_size) {
			i = 0;
			swap_buffers(buffer_size);
		}
	}
	if (i != 0) swap_buffers(i);
	fclose(bin_file);
}

void BufferedPointReader::start() {
	std::thread read_thread([this] {
		read();
		while (points_available()); // Wait for all points to be read
		_eof = true;
		buffer_lock.lock(); // Wait for mutex to be unlocked by the main thread to safely terminate this thread
		buffer_lock.unlock();

		delete[] buffer0;
		delete[] buffer1;
		_done.store(true);
	});
	read_thread.detach();
}

BufferedPointReader::BufferedPointReader(std::string file, uint8_t format, uint64_t buffer_size) {
	this->file = file;
	this->format = format;
	this->buffer_size = buffer_size;

	points_in_buffer = 0;
	las_total_points = 0;
	las_legacy_total_points = 0;
	las_num_points = 0;
	las_skip_bytes = 0;
	las_first_point = 0;

	las_scale_x = 0.0;
	las_scale_y = 0.0;
	las_scale_z = 0.0;
	las_offset_x = 0.0;
	las_offset_y = 0.0;
	las_offset_z = 0.0;

	public_buffer = &buffer0;
	private_buffer = &buffer1;
	_public_buffer = false;

	buffer0 = new Point[buffer_size];
	buffer1 = new Point[buffer_size];

	_points_available = false;
	_eof = false;
	_done = false;
}