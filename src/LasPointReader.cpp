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

	fseek(file, 104, SEEK_SET);
	fread(&point_format, sizeof(uint8_t), 1, file);

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
	if (!(cx && cy && cz)) throw std::runtime_error("Unexpected end of file");

	p.x = x * scale_x + offset_x;
	p.y = y * scale_y + offset_y;
	p.z = z * scale_z + offset_z;

	if (point_format == 2) { // It has colors!
		fseek(file, skip_bytes - 3 * sizeof(uint16_t), SEEK_CUR);
		cx = fread(&p.r, sizeof(uint16_t), 1, file);
		cy = fread(&p.g, sizeof(uint16_t), 1, file);
		cz = fread(&p.b, sizeof(uint16_t), 1, file);
	}
	else {
		fseek(file, skip_bytes, SEEK_CUR);
		p.r = 0;
		p.g = 0;
		p.b = 0;
	}
	if (!(cx && cy && cz)) throw std::runtime_error("Unexpected end of file");

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

Bounds LasPointReader::get_bounds() {
	return { (float)min_x, (float)min_y, (float)min_z, (float)max_x, (float)max_y, (float)max_z };
}

Cube LasPointReader::get_big_bounding_cube(std::vector<std::string> input_files, uint64_t& total_points) {
	Bounds g_bounds;
	for (std::string s : input_files) {
		LasPointReader r;
		r.open(s);
		total_points += r.num_points;
		Bounds b = r.get_bounds();
		if (b.max_x > g_bounds.max_x) g_bounds.max_x = b.max_x;
		if (b.max_y > g_bounds.max_y) g_bounds.max_y = b.max_y;
		if (b.max_z > g_bounds.max_z) g_bounds.max_z = b.max_z;

		if (b.min_x < g_bounds.min_x) g_bounds.min_x = b.min_x;
		if (b.min_y < g_bounds.min_y) g_bounds.min_y = b.min_y;
		if (b.min_z < g_bounds.min_z) g_bounds.min_z = b.min_z;
	}
	Cube c;
	// Cast to double to avoid overflow when adding
	c.center_x = (float)(((double)g_bounds.max_x + (double)g_bounds.min_x) / 2.0);
	c.center_y = (float)(((double)g_bounds.max_y + (double)g_bounds.min_y) / 2.0);
	c.center_z = (float)(((double)g_bounds.max_z + (double)g_bounds.min_z) / 2.0);

	c.size = std::max(g_bounds.max_x - g_bounds.min_x,
		std::max(g_bounds.max_y - g_bounds.min_y, g_bounds.max_z - g_bounds.min_z));
	return c;
}