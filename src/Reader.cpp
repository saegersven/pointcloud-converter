#include <filesystem>
#include <iostream>
#include "Reader.h"
#include "utils.h"

Reader::Reader(std::string input_path, std::string output_path)
{
	this->input_path = input_path;
	this->output_path = output_path;
}

/// <summary>
/// Read through a .pts file and calculate an Axis-Aligned Bounding Cube.
/// Also rewrite all the points into a binary file, since they have to be read again later on.
/// </summary>
/// <returns>An AABB that surrounds all points</returns>
Cube Reader::read_bounds() {
	std::ifstream fin(input_path);

	Bounds bounds;

	if (!fin.is_open()) throw std::exception("Could not read points");

	// To make reading faster for the next step, copy the contents of this file into a binary file
	temp_point_path = get_full_point_file("", output_path, "");

	FILE* point_file;
	fopen_s(&point_file, temp_point_path.c_str(), "wb");
	if (!point_file) throw std::exception("Could not copy points");

	fseek(point_file, 0, SEEK_SET);

	uint64_t num_points;
	fin >> num_points;

	Point p;

	int dummy;
	fin >> dummy; // Read number of points first

	for (int i = 0; (fin >> p.x >> p.y >> p.z); i++) {
		if (p.x < bounds.min_x) bounds.min_x = p.x;
		if (p.x > bounds.max_x) bounds.max_x = p.x;

		if (p.y < bounds.min_y) bounds.min_y = p.y;
		if (p.y > bounds.max_y) bounds.max_y = p.y;

		if (p.z < bounds.min_z) bounds.min_z = p.z;
		if (p.z > bounds.max_z) bounds.max_z = p.z;
		fwrite(&p, sizeof(p), 1, point_file);

		// Read additional values
		fin >> dummy;// >> dummy >> dummy >> dummy;
	}

	fclose(point_file);

	// Create Axis-Aligned Bounding Cube
	Cube c;
	c.center_x = (bounds.min_x + bounds.max_x) / 2.0f;
	c.center_y = (bounds.min_y + bounds.max_y) / 2.0f;
	c.center_z = (bounds.min_z + bounds.max_z) / 2.0f;

	c.size = 0.01f + std::max(bounds.max_x - bounds.min_x,
		std::max(bounds.max_y - bounds.min_y, bounds.max_z - bounds.min_z)) / 2.0f;

	return c;
}

/// <summary>
/// Split points into eight smaller chunks of points
/// </summary>
/// <param name="bounding_cube">The bounding cube of all points. Needed to determine which chunk a point belongs to.</param>
/// <param name="point_file_path">The path to the point file</param>
/// <returns>Data about the eight smaller chunks</returns>
SplitPointsMetadata Reader::split_points(Cube bounding_cube, std::string point_file_path) {
	SplitPointsMetadata splitPointsMetadata;
	splitPointsMetadata.bounding_cubes = std::vector<Cube>(8);

	for (int i = 0; i < 8; i++) {
		Cube c;
		c.center_x = -(bounding_cube.size / 2.0f) + ((i & (1 << 2)) ? 1 : 0) * bounding_cube.size + bounding_cube.center_x;
		c.center_y = -(bounding_cube.size / 2.0f) + ((i & (1 << 1)) ? 1 : 0) * bounding_cube.size + bounding_cube.center_y;
		c.center_z = -(bounding_cube.size / 2.0f) + ((i & (1 << 0)) ? 1 : 0) * bounding_cube.size + bounding_cube.center_z;
		c.size = bounding_cube.size / 2.0f;
		splitPointsMetadata.bounding_cubes[i] = c;
	}
	
	FILE* point_file;
	fopen_s(&point_file, point_file_path.c_str(), "rb");
	if (!point_file) throw std::exception("Could not read points");

	std::vector<FILE*> point_files(8);
	splitPointsMetadata.point_file_paths = std::vector<std::string>(8);

	for (int i = 0; i < 8; i++) {
		splitPointsMetadata.point_file_paths[i] = point_file_path + std::to_string(i);
		fopen_s(&point_files[i], splitPointsMetadata.point_file_paths[i].c_str(), "wb");
		if (!point_files[i]) throw std::exception("Could not write points");
	}

	Point p;
	while (fread(&p, sizeof(p), 1, point_file)) {
		int index = 0;
		index |= (p.x > bounding_cube.center_x) ? (1 << 2) : 0;
		index |= (p.y > bounding_cube.center_y) ? (1 << 1) : 0;
		index |= (p.z > bounding_cube.center_z) ? (1 << 0) : 0;
		fwrite(&p, sizeof(p), 1, point_files[index]);
		splitPointsMetadata.num_points[index]++;
	}

	for (int i = 0; i < 8; i++) {
		fclose(point_files[i]);
	}

	fclose(point_file);

	remove(point_file_path.c_str());

	return splitPointsMetadata;
}