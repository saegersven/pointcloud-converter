#pragma once
#include <string>
#include <mutex>
#include <thread>
#include "Data.h"

class BufferedPointReader {
private:
	std::string file;
	uint8_t format;
	uint64_t buffer_size;

	uint64_t points_in_buffer;

	// LAS SPECIFIC
	uint64_t las_total_points;
	uint32_t las_legacy_total_points;
	uint64_t las_num_points;
	uint16_t las_skip_bytes;
	uint32_t las_first_point;

	double las_scale_x, las_scale_y, las_scale_z;
	double las_offset_x, las_offset_y, las_offset_z;
	// ------------

	std::mutex buffer_lock;
	Point** public_buffer;
	Point** private_buffer;
	bool _public_buffer;

	Point* buffer0;
	Point* buffer1;

	std::atomic<bool> _points_available;
	std::atomic<bool> _eof;

	bool read_point_raw(FILE* file, Point& p);
	bool open_file_raw(FILE*& f);

	bool read_point_las(FILE* file, Point& p);
	bool open_file_las(FILE*& f);

	void read();
	void swap_buffers(uint64_t num_points);

public:
	BufferedPointReader(std::string file, uint8_t format, uint64_t buffer_size);
	void start();

	uint64_t start_reading(Point*& buffer);
	void stop_reading();

	bool points_available();
	bool eof();
};