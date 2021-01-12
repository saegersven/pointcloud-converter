#include "LasPointReader.h"
#include <stdexcept>

void LasPointReader::open(std::string filename) {
	file = fopen(filename.c_str(), "rb");
	if (!file) throw std::runtime_error("Could not open file");

	// Read header
	fseek(file, 96, SEEK_SET);
	fread(&first_point_offset, sizeof(uint32_t), 1, file);

	fseek(file, 5, SEEK_CUR);
	fread(&skip_bytes, sizeof(uint16_t), 1, file);
	skip_bytes -= 12; // Subtract size of coordinates

	uint32_t legacy_num_points = 0;
	fread(&legacy_num_points, sizeof(uint32_t), 1, file);
	if (legacy_num_points == 0) { // This file uses the 64 bit num_points
		fseek(file, 140, SEEK_SET);
		fread(&num_points, sizeof(uint64_t), 1, file);
	}
	else {
		num_points = legacy_num_points;
	}

	fseek(file, 131, SEEK_SET);
	fread(&scale_x, sizeof(double), 1, file);
	fread(&scale_y, sizeof(double), 1, file);
	fread(&scale_z, sizeof(double), 1, file);

	fread(&offset_x, sizeof(double), 1, file);
	fread(&offset_y, sizeof(double), 1, file);
	fread(&offset_z, sizeof(double), 1, file);

	fread(&max_x, sizeof(double), 1, file);
	fread(&min_x, sizeof(double), 1, file);
	fread(&max_y, sizeof(double), 1, file);
	fread(&min_y, sizeof(double), 1, file);
	fread(&max_z, sizeof(double), 1, file);
	fread(&min_z, sizeof(double), 1, file);

	fseek(file, first_point_offset, SEEK_SET); // Jump to the first point to continue
}

bool LasPointReader::has_points() {
	return points_read < num_points;
}

Point LasPointReader::read_point() {
	Point p;

	bool cx, cy, cz;
	int32_t x, y, z;
	cx = fread(&x, sizeof(int32_t), 1, file);
	cy = fread(&y, sizeof(int32_t), 1, file);
	cz = fread(&z, sizeof(int32_t), 1, file);
	fseek(file, skip_bytes, SEEK_CUR);
	if (!(cx && cy && cz)) throw std::runtime_error("Unexpected end of file");

	p.x = x * scale_x + offset_x;
	p.y = y * scale_y + offset_y;
	p.z = z * scale_z + offset_z;

	points_read++;

	return p;
}

Cube LasPointReader::get_bounding_cube() {
	Cube c;
	c.center_x =(float)((max_x + min_x) / 2.0);
	c.center_y =(float)((max_y + min_y) / 2.0);
	c.center_z =(float)((max_z + min_z) / 2.0);

	c.size = std::max(max_x - min_x, std::max(max_y - min_y, max_z - min_z));
	return c;
}