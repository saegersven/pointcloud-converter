#pragma once
#include "PointReader.h"

class LasPointReader : public PointReader {
private:
	uint64_t num_points;
	uint64_t points_read = 0;
	double scale_x, scale_y, scale_z;
	double offset_x, offset_y, offset_z;
	double min_x, max_x, min_y, max_y, min_z, max_z;
	uint32_t first_point_offset;
	uint16_t skip_bytes;
	uint8_t point_format;

public:
	void open(std::string filename) override;
	bool has_points() override;
	Point read_point() override;

	Cube get_bounding_cube();
};