#include "Reader.h"

Reader::Reader(std::vector<std::string> input_path, std::string output_path)
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
	Bounds bounds;

	// To make reading faster for the next step, copy the contents of this file into a binary file
	temp_point_path = get_full_point_file("", output_path);

	/*FILE* point_file;
	fopen_s(&point_file, temp_point_path.c_str(), "wb");
	if (!point_file) throw std::exception("Could not copy points");

	fseek(point_file, 0, SEEK_SET);*/

	BufferedPointWriter writer(output_path, 1024);

	const uint64_t buf_size = 100'000;
	uint64_t i = 0;
	for (std::string input : input_path) {

		BufferedPointReader reader(input, POINT_FILE_FORMAT_LAS, buf_size);
		reader.start();

		writer.start_writing();

		std::vector<Point> point_buffer;
		point_buffer.resize(buf_size);

		Point* points;
		uint64_t num_points = 1;

		const int status_interval = 1'000'000;
		while (true) { // reader.start_reading will return 0 if all points have been read
			num_points = reader.start_reading(points);
			if (num_points == 0) {
				writer.schedule_points("", point_buffer);
				reader.stop_reading();
				break;
			}

			if (num_points != point_buffer.size()) {
				point_buffer.resize(num_points);
			}

			for (uint64_t y = 0; y < num_points; y++) {

				if (points[y].x < bounds.min_x) bounds.min_x = points[y].x;
				if (points[y].x > bounds.max_x) bounds.max_x = points[y].x;

				if (points[y].y < bounds.min_y) bounds.min_y = points[y].y;
				if (points[y].y > bounds.max_y) bounds.max_y = points[y].y;

				if (points[y].z < bounds.min_z) bounds.min_z = points[y].z;
				if (points[y].z > bounds.max_z) bounds.max_z = points[y].z;

				//fwrite(&p, sizeof(p), 1, point_file);
				point_buffer[y] = points[y];
				i++;
				if (i % status_interval == 0) Logger::log_return(std::to_string(i) + " points");
			}

			writer.schedule_points("", point_buffer);

			reader.stop_reading();
		}
	}
	writer.done();
	Logger::log_info(std::to_string(i) + " total points read");

	while (!writer.finished()); // Wait until the writer has finished cleaning up before it gets destroyed

	// Create Axis-Aligned Bounding Cube
	Cube c;
	c.center_x = (bounds.min_x + bounds.max_x) / 2.0f;
	c.center_y = (bounds.min_y + bounds.max_y) / 2.0f;
	c.center_z = (bounds.min_z + bounds.max_z) / 2.0f;

	c.size = 0.01f + std::max(bounds.max_x - bounds.min_x,
		std::max(bounds.max_y - bounds.min_y, bounds.max_z - bounds.min_z)) / 2.0f;

	return c;
}

void Reader::read_bounds_las(std::vector<std::string> input_files, Cube& bounds, uint64_t& num_points) {
	double max_x = std::numeric_limits<double>::lowest();
	double min_x = std::numeric_limits<double>::max();
	double max_y = std::numeric_limits<double>::lowest();
	double min_y = std::numeric_limits<double>::max();
	double max_z = std::numeric_limits<double>::lowest();
	double min_z = std::numeric_limits<double>::max();

	for (int i = 0; i < input_files.size(); i++) {
		// Read las header
		FILE* file = fopen(input_files[i].c_str(), "rb");
		if (!file) throw std::runtime_error("Could not open file");

		uint64_t num_points_file = 0;
		uint32_t num_points_file_legacy = 0;
		fseek(file, 107, SEEK_SET);
		fread(&num_points_file_legacy, sizeof(uint32_t), 1, file);
		if (num_points_file_legacy == 0) {
			fseek(file, 140, SEEK_SET);
			fread(&num_points_file, sizeof(uint64_t), 1, file);
			fseek(file, 107 + sizeof(uint32_t), SEEK_SET);
			num_points += num_points_file;
		}
		else {
			num_points += num_points_file_legacy;
		}

		double bounds_dummy;
		fseek(file, 179, SEEK_SET);
		fread(&bounds_dummy, sizeof(double), 1, file); // Max X
		if (bounds_dummy > max_x) max_x = bounds_dummy;
		fread(&bounds_dummy, sizeof(double), 1, file); // Min X
		if (bounds_dummy < min_x) min_x = bounds_dummy;

		fread(&bounds_dummy, sizeof(double), 1, file); // Max Y
		if (bounds_dummy > max_y) max_y = bounds_dummy;
		fread(&bounds_dummy, sizeof(double), 1, file); // Min Y
		if (bounds_dummy < min_y) min_y = bounds_dummy;

		fread(&bounds_dummy, sizeof(double), 1, file); // Max Z
		if (bounds_dummy > max_z) max_z = bounds_dummy;
		fread(&bounds_dummy, sizeof(double), 1, file); // Min Z
		if (bounds_dummy < min_z) min_z = bounds_dummy;
	}
	bounds.center_x = (float)((min_x + max_x) / 2.0);
	bounds.center_y = (float)((min_y + max_y) / 2.0);
	bounds.center_z = (float)((min_z + max_z) / 2.0);
	bounds.size = (float)(0.01 + std::max(max_x - min_x, std::max(max_y - min_y, max_z - min_z)) / 2.0);
}

/// <summary>
/// Split points into eight smaller chunks of points
/// </summary>
/// <param name="bounding_cube">The bounding cube of all points. Needed to determine which chunk a point belongs to.</param>
/// <param name="point_file_path">The path to the point file</param>
/// <returns>Data about the eight smaller chunks</returns>
SplitPointsMetadata Reader::split_points(Cube bounding_cube) {
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
	
	/*FILE* point_file;
	fopen_s(&point_file, get_full_point_file("", output_path, "").c_str(), "rb");
	if (!point_file) throw std::exception("Could not read points");*/

	/*std::vector<FILE*> point_files(8);
	splitPointsMetadata.point_file_paths = std::vector<std::string>(8);

	for (int i = 0; i < 8; i++) {
		splitPointsMetadata.point_file_paths[i] = get_full_point_file(std::to_string(i), output_path, "");
		fopen_s(&point_files[i], splitPointsMetadata.point_file_paths[i].c_str(), "wb");
		if (!point_files[i]) throw std::exception("Could not write points");
	}*/

	/*Point p;
	while (fread(&p, sizeof(p), 1, point_file)) {
		int index = 0;
		index |= (p.x > bounding_cube.center_x) ? (1 << 2) : 0;
		index |= (p.y > bounding_cube.center_y) ? (1 << 1) : 0;
		index |= (p.z > bounding_cube.center_z) ? (1 << 0) : 0;
		fwrite(&p, sizeof(p), 1, point_files[index]);
		splitPointsMetadata.num_points[index]++;
	}*/

	BufferedPointReader reader(get_full_point_file("", output_path), POINT_FILE_FORMAT_RAW, 200'000);
	reader.start();

	BufferedPointWriter writer(output_path, 20'000);
	writer.start_writing();

	std::vector<std::vector<Point>> local_buf(8);
	for (int i = 0; i < 8; i++) {
		local_buf[i].reserve(200'000);
	}

	Point* points;
	uint64_t num_points = 0;
	while (num_points = reader.start_reading(points)) { // reader.start_reading will return 0 if all points have been read
		for (uint64_t i = 0; i < num_points; i++) {
			uint8_t index = 0;
			index |= (points[i].x > bounding_cube.center_x) ? (1 << 2) : 0;
			index |= (points[i].y > bounding_cube.center_y) ? (1 << 1) : 0;
			index |= (points[i].z > bounding_cube.center_z) ? (1 << 0) : 0;

			local_buf[index].push_back(points[i]);
			//fwrite(&points[0], sizeof(struct Point), 1, point_files[index]);
			splitPointsMetadata.num_points[index]++;
		}
		reader.stop_reading();
		for (int i = 0; i < 8; i++) {
			writer.schedule_points(std::to_string(i), local_buf[i]);
			local_buf[i].clear();
		}
	}
	writer.done();

	while (!writer.finished()); // Wait until writer is finished cleaning up

	//fclose(point_file);

	try {
		std::filesystem::remove(get_full_point_file("", output_path));
	}
	catch (std::filesystem::filesystem_error e) {
		throw std::runtime_error("Error deleting file");
	}

	return splitPointsMetadata;
}