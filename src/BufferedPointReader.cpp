#include "BufferedPointReader.h"

SharedPointBuffer* BufferedPointReader::get_buffer(bool buffer_index) {
	if (buffer_index) return &buffer1;
	return &buffer0;
}

void BufferedPointReader::swap(bool write) {
	if (write) {
		write_buffer_b = !write_buffer_b;
		write_buffer = get_buffer(write_buffer_b);
	}
	else if (!write) {
		read_buffer_b = !read_buffer_b;
		read_buffer = get_buffer(read_buffer_b);
	}
}

void BufferedPointReader::swap_and_get(std::vector<Point>& buf, uint64_t& num_points) {
	read_buffer->buffer_lock.unlock();
	swap(false);
	read_buffer->buffer_lock.lock();
	buf = read_buffer->buffer;
	num_points = read_buffer->buffer_size;

	if (read_buffer_b) {
		_points_available_1 = false;
	}
	else {
		_points_available_0 = false;
	}

	if (!_is_reading) read_buffer->buffer_lock.unlock();
}

void BufferedPointReader::read_async() {
	FILE* file;
	if (fopen_s(&file, file_path.c_str(), "rb") != NULL || !file) throw std::exception("Could not open file");

	write_buffer->buffer_lock.lock();
	bool c = true;
	while (c) {
		write_buffer->buffer_size = 0;

		Point p;
		// Read until the point limit or eof is reached.
		while (write_buffer->buffer_size < point_limit) {
			if (!fread(&p, sizeof(struct Point), 1, file)) {
				// Reached end of file
				c = false;
				break;
			}
			write_buffer->buffer[write_buffer->buffer_size] = p;
			write_buffer->buffer_size++;
		}

		status_lock.lock();
		if (write_buffer_b) {
			_points_available_1 = true;
		} else {
			_points_available_0 = true;
		}
		_is_reading = true;
		status_lock.unlock();

		write_buffer->buffer_lock.unlock();
		swap(true);
		write_buffer->buffer_lock.lock();
	}
	write_buffer->buffer_lock.unlock();
	fclose(file);

	status_lock.lock();
	_is_reading = false;
	status_lock.unlock();

	while (points_available()); // Wait until all points have been read
}

void BufferedPointReader::start_reading() {
	read_buffer->buffer_lock.lock();
	/*std::async(std::launch::async, [this]() {
		this->read_async();
	});*/
	std::thread read_thread([this] { this->read_async(); });
	read_thread.detach();
}

BufferedPointReader::BufferedPointReader(std::string file_path, uint64_t point_limit)
	: buffer0(point_limit), buffer1(point_limit) {

	write_buffer_b = false;
	read_buffer_b = true;
	write_buffer = get_buffer(write_buffer_b);
	read_buffer = get_buffer(read_buffer_b);

	this->file_path = file_path;
	this->point_limit = point_limit;

	_points_available_0 = false;
	_points_available_1 = false;
}

void BufferedPointReader::cleanup() {

}

bool BufferedPointReader::points_available() {
	status_lock.lock();
	bool b = _points_available_0 || _points_available_1;
	status_lock.unlock();
	return b;
}

bool BufferedPointReader::is_reading() {
	status_lock.lock();
	bool b = _is_reading;
	status_lock.unlock();
	return b;
}