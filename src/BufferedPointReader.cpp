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
<<<<<<< HEAD
<<<<<<< HEAD
	FILE* file = nullptr;

	double las_scale_x, las_scale_y, las_scale_z = 0.0;
	uint32_t las_num_points = 0;
	uint32_t las_points_read = 0;

	std::ifstream fin;

	int dummy;
	if (format == FILE_FORMAT_RAW || format == FILE_FORMAT_LAS227) {
#ifdef _BUILD_CMAKE
		file = fopen(file_path.c_str(), "rb");
#else
		if (fopen_s(&file, file_path.c_str(), "rb") != NULL) throw std::exception("Could not open file");
#endif
		if(!file) throw std::exception("Could not open file");

		if (format == FILE_FORMAT_LAS227) {
			fseek(file, 107, SEEK_SET);
			fread(&las_num_points, sizeof(las_num_points), 1, file);

			fseek(file, 131, SEEK_SET);
			// Read scale
			fread(&las_scale_x, sizeof(double), 1, file);
			fread(&las_scale_y, sizeof(double), 1, file);
			fread(&las_scale_z, sizeof(double), 1, file);

			fseek(file, 227, SEEK_SET);
		}
=======
=======
>>>>>>> parent of 40c21ca... Added cmake
	FILE* file;
	std::ifstream* fin;
	if (binary) {
		if (fopen_s(&file, file_path.c_str(), "rb") != NULL || !file) throw std::exception("Could not open file");
<<<<<<< HEAD
>>>>>>> parent of 40c21ca... Added cmake
=======
>>>>>>> parent of 40c21ca... Added cmake
	}
	else {
		fin = &std::ifstream(file_path);
		if (!fin->is_open()) throw std::exception("Could not open file");
	}

	write_buffer->buffer_lock.lock();
	bool c = true;
	while (c) {
		write_buffer->buffer_size = 0;

		Point p;
		// Read until the point limit or eof is reached.
		while (write_buffer->buffer_size < point_limit) {
			if (binary) {
				if (!fread(&p, sizeof(struct Point), 1, file)) {
					// Reached end of file
					c = false;
					break;
				}
			}
			else {
				if (!(*fin >> p.x >> p.y >> p.z)) {
					c = false;
					break;
				}
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
	if (binary) {
		fclose(file);
	}
	else {
		fin->close();
	}

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

BufferedPointReader::BufferedPointReader(bool binary, std::string file_path, uint64_t point_limit)
	: buffer0(point_limit), buffer1(point_limit) {

	write_buffer_b = false;
	read_buffer_b = true;
	write_buffer = get_buffer(write_buffer_b);
	read_buffer = get_buffer(read_buffer_b);

	this->binary = binary;
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