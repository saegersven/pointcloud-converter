#include <string>
#include <mutex>
#include "Data.h"

class BufferedPointReader {
private:
	std::string file;
	uint8_t format;
	uint64_t buffer_size;

	uint64_t points_in_buffer;

	FILE* bin_file;

	std::mutex buffer_lock;
	Point** public_buffer;
	Point** private_buffer;
	bool _public_buffer;

	Point* buffer0;
	Point* buffer1;

	bool _points_available;
	bool _eof;

	bool read_point_raw(Point& p);
	bool open_file_raw();

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