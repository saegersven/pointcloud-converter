#include "Data.h"
#include <mutex>
#include <future>
#include <fstream>

struct SharedPointBuffer {
	std::vector<Point> buffer;
	uint64_t buffer_size;
	std::mutex buffer_lock;

	SharedPointBuffer(uint64_t size) : buffer(size) {
		buffer_size = 0;
	}
};

class BufferedPointReader {
private:
	bool binary;

	std::mutex status_lock;
	bool _points_available_0;
	bool _points_available_1;

	bool _is_reading;

	std::string file_path;
	uint64_t point_limit;

	SharedPointBuffer buffer0;
	SharedPointBuffer buffer1;

	SharedPointBuffer* read_buffer;
	SharedPointBuffer* write_buffer;

	bool read_buffer_b;
	bool write_buffer_b;

	SharedPointBuffer* get_buffer(bool write);
	void swap(bool write);
	void read_async();
public:
	void swap_and_get(std::vector<Point>& buf, uint64_t& num_points);
	BufferedPointReader(bool binary, std::string file_path, uint64_t point_limit);
	void cleanup();
	void start_reading();
	bool points_available();
	bool is_reading();
};