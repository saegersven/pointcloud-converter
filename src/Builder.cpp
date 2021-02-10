#include "Builder.h"

void Builder::ic_load_points(Node* node) {
	FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
	if (!points_file) throw std::runtime_error("Could not open file");

	node->points.resize(node->num_points);

	Point p;
	for (uint64_t i = 0; i < node->num_points; i++) {
		if (!fread(&p, sizeof(struct Point), 1, points_file)) break;
		node->points[i] = p;
	}

	fclose(points_file);
}

uint8_t Builder::find_child_node_index(Cube& bounds, Point& p) {
	uint8_t index = 0;
	if (p.x > bounds.center_x) index |= (1 << 2);
	if (p.y > bounds.center_y) index |= (1 << 1);
	if (p.z > bounds.center_z) index |= (1 << 0);
	return index;
}

Node* Builder::create_child_node(std::string id, uint64_t num_points, std::vector<Point> points, float center_x, float center_y, float center_z, float size) {
	Node* node = new Node();
	node->id = id;
	node->num_points = num_points;
	node->points = points;
	node->bounds.center_x = center_x;
	node->bounds.center_y = center_y;
	node->bounds.center_z = center_z;
	node->bounds.size = size;
	return node;
}

uint64_t Builder::ic_sample_node(Node* node) {
	/*FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "wb");
	if (!points_file) throw std::runtime_error("Could not open file");*/

	uint64_t to_sample = (std::min(sampled_node_size, (uint32_t)node->points.size()));
	uint64_t sample_interval = node->points.size() / to_sample;

	std::vector<Point> sampled_points;
	sampled_points.resize(to_sample);

	for (uint64_t i = 0; i < to_sample; i++) {
		//fwrite(&node->points[i * sample_interval], sizeof(struct Point), 1, points_file);
		//sampled_points[i] = node->points[i * sample_interval];
		sampled_points[i] = node->points[i];
	}

	//fclose(points_file);
	node->points.swap(sampled_points);
	node->num_points = node->points.size();

	//writer.add_num_points_in_core(node->points.size());
	num_points_in_core += node->points.size();

	write_node(node, true);

	return to_sample;
}

void Builder::ic_split_node(Node* node, bool is_async) {
	if (node->num_points > max_node_size) {
		if (node->num_points > 1'000'000 && !is_async) {
			futures.push_back(std::async(std::launch::async, [this, node] {
				Logger::add_thread_alias("BLDA");
				ic_split_node(node, true);
			}));
			return;
		}
		std::vector<Point> child_points[8];
		/*for (int i = 0; i < 8; i++) {
			child_points[i].reserve(node->num_points / 4);
		}*/

		for (uint64_t i = 0; i < node->num_points; i++) {
			child_points[find_child_node_index(node->bounds, node->points[i])].push_back(node->points[i]);
		}

		node->child_nodes = new Node*[8];
		for (int i = 0; i < 8; i++) {
			if (child_points[i].size() != 0) {
				std::string id = node->id;
				id.append(std::to_string(i));
				Node* child_node = create_child_node(id, child_points[i].size(), child_points[i],
					node->bounds.center_x + (-(node->bounds.size / 2.0f) + ((i & (1 << 2)) ? node->bounds.size : 0)),
					node->bounds.center_y + (-(node->bounds.size / 2.0f) + ((i & (1 << 1)) ? node->bounds.size : 0)),
					node->bounds.center_z + (-(node->bounds.size / 2.0f) + ((i & (1 << 0)) ? node->bounds.size : 0)),
					node->bounds.size / 2.0f);

				node->child_nodes_mask |= (1 << i);
				node->child_nodes[i] = child_node;
			}
		}

		node->num_points = ic_sample_node(node);

		for (int i = 0; i < 8; i++) {
			if (node->child_nodes_mask & (1 << i)) {
				ic_split_node(node->child_nodes[i], false);
			}
		}
	}
	else {
		if (!node->points.size()) throw std::runtime_error("No points loaded");
		/*FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "wb");
		if (!points_file) throw std::runtime_error("Could not open file");

		for (uint64_t i = 0; i < node->num_points; i++) {
			fwrite(&node->points[i], sizeof(struct Point), 1, points_file);
		}
		std::vector<Point>().swap(node->points); // Clear points and free memory

		fclose(points_file);*/
		write_node(node, true);
		points_processed += node->num_points;
	}
}

void Builder::write_node(Node* node, bool in_core) {
	pool.add_job([this, node, in_core] {
		while (open_octree_files >= 32) std::this_thread::sleep_for(std::chrono::milliseconds(10));

		FILE* octree_file = fopen(octree_file_path.c_str(), "wb");
		if (!octree_file) THROW_FILE_OPEN_ERROR;

		octree_file_lock.lock();
		open_octree_files++;
		node->byte_index = octree_file_cursor;
		octree_file_cursor += node->num_points * sizeof(struct Point);
		octree_file_lock.unlock();

		fseek(octree_file, node->byte_index, SEEK_SET);
		if (in_core) {
			fwrite(&node->points[0], sizeof(struct Point), node->points.size(), octree_file);

			std::vector<Point>().swap(node->points);
			num_points_in_core -= node->points.size();
		}
		else {
			std::string path = get_full_point_file(node->id, output_path);
			FILE* node_points_file = fopen(path.c_str(), "rb");
			if (!node_points_file) THROW_FILE_OPEN_ERROR;

			for (uint64_t i = 0; i < node->num_points; i++) {
				Point p;
				fread(&p, sizeof(struct Point), 1, node_points_file);
				fwrite(&p, sizeof(struct Point), 1, octree_file);
			}
			fclose(node_points_file);

			std::filesystem::remove(path);
		}
		fclose(octree_file);
		open_octree_files--;
	});
	//writer.add(node, in_core);
}

void Builder::split_node(Node* node, bool is_async) {
	split_node(node, is_async, false, std::vector<std::string>());
}

void Builder::split_node(Node* node, bool is_async, bool is_las, std::vector<std::string> input_files) {
	if (node->num_points > max_node_size) {
		if (node->num_points < 5'000'000) {
			if (num_points_in_core < 75'000'000) { // Ensure that a maximum of ~80M points are in memory at the same time
				// Split this node in-core
				ic_load_points(node);
				num_points_in_core += node->points.size();

				std::filesystem::remove(get_full_point_file(node->id, output_path));

				ic_split_node(node, false);
				return;
			}
		}
		else if (node->num_points > 7'500'000 && !is_async) {
			// Run async
			pool.add_job([this, node] {
				Logger::add_thread_alias("BLDA");
				try {
					split_node(node, true);
				}
				catch (const std::exception exc) {
					Logger::log_error("Error splitting Node: " + std::string(exc.what()));
				}
			});
			return;
		}


		/*FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
		if (!points_file) throw std::runtime_error("Could not open file");*/
		if (!is_las) {
			input_files.push_back(get_full_point_file(node->id, output_path));
		}

		// Since we will split this node, we can sample it now
		FILE* sample_file = fopen(get_full_temp_point_file(node->id, output_path).c_str(), "wb");
		if (!sample_file) throw std::runtime_error("Could not open file");

		FILE* child_point_files[8] = { nullptr };
		uint64_t num_child_points[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		uint64_t i = 0;
		uint64_t sampled_points = 0;
		uint64_t sample_interval = (int)((double)node->num_points / (double)sampled_node_size);

		for (std::string file : input_files) {
			if (input_files.size() > 1) Logger::log_info("Reading file '" + std::filesystem::path(file).filename().string() + "'");
			//BufferedPointReader reader(file, is_las ? POINT_FILE_FORMAT_LAS : POINT_FILE_FORMAT_RAW, 1'000'000);
			std::unique_ptr<PointReader> r;
			if (is_las) {
				r = std::unique_ptr<LasPointReader>(new LasPointReader);
			}
			else {
				r = std::unique_ptr<RawPointReader>(new RawPointReader);
			}
			r->open(file);

			for (Point p = r->read_point(); r->has_points(); p = r->read_point()) {
				if (i % sample_interval == 0) {
					fwrite(&p, sizeof(struct Point), 1, sample_file);
					sampled_points++;
				}

				uint8_t index = find_child_node_index(node->bounds, p);
				if (!child_point_files[index]) {
					child_point_files[index] = fopen(get_full_point_file(node->id + std::to_string(index), output_path).c_str(), "wb");
					if (!child_point_files[index]) throw std::runtime_error("Could not open file");
				}
				fwrite(&p, sizeof(struct Point), 1, child_point_files[index]);
				num_child_points[index]++;
				i++;
			}
		}
		fclose(sample_file);

		// If this file exists (if we have not read from an las file), remove it so it can be replaced with the sampled points file
		if(!is_las) std::filesystem::remove(get_full_point_file(node->id, output_path));
		// Replace the file that contains all points with the temp file that contains the sampled subset
		std::filesystem::rename(get_full_temp_point_file(node->id, output_path), get_full_point_file(node->id, output_path));
		node->num_points = sampled_points;

		write_node(node, false);

		node->child_nodes = new Node*[8];
		for (int i = 0; i < 8; i++) {
			if (child_point_files[i]) {
				fclose(child_point_files[i]);
				std::string id = node->id;
				id.append(std::to_string(i));
				Node* child_node = create_child_node(id, num_child_points[i], std::vector<Point>(),
					node->bounds.center_x + (-(node->bounds.size / 2.0f) + ((i & (1 << 2)) ? node->bounds.size : 0)),
					node->bounds.center_y + (-(node->bounds.size / 2.0f) + ((i & (1 << 1)) ? node->bounds.size : 0)),
					node->bounds.center_z + (-(node->bounds.size / 2.0f) + ((i & (1 << 0)) ? node->bounds.size : 0)),
					node->bounds.size / 2.0f);

				node->child_nodes_mask |= (1 << i);
				node->child_nodes[i] = child_node;

				split_node(child_node, false);
			}
		}
	} else {
		write_node(node, false);
		points_processed += node->num_points;
	}
}

Node* Builder::build() {
	Node* root_node = new Node();
	root_node->id = "";
	root_node->bounds = bounding_cube;
	root_node->child_nodes_mask = 0;
	root_node->num_points = num_points;

	bool status_terminated = false;
	std::thread status_thread([this, root_node, status_terminated] {
		Logger::add_thread_alias("BUILD");
		std::chrono::milliseconds wait_for(3000);
		uint64_t total_points = root_node->num_points;
		while (points_processed < total_points && !status_terminated) {
			uint64_t points = points_processed.load();
			Logger::log_return(std::to_string((int)((double)points / (double)total_points * 100.0)) + "% ("
				+ std::to_string(points) + "/" + std::to_string(total_points) + ")\r");
			std::this_thread::sleep_for(wait_for);
		}
	});
	status_thread.detach();

	//writer.start(output_path);

	split_node(root_node, true /*Don't make the root node async*/,
		true /*The root node is directly split from the input las files*/, las_input_paths);

	/*std::chrono::milliseconds wait_span(500);
	while (futures.size() > 0) {
		for (int i = 0; i < futures.size(); i++) {
			if (futures[i].wait_for(wait_span) != std::future_status::ready) {
				continue;
			}
			futures[i].get();
			futures.erase(futures.begin() + i);
			i--;
		}
	}*/
	//pool.wait();
	//writer.done();

	Logger::log_info("Done building                       ");

	return root_node;
}

Builder::Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
	uint32_t max_node_size, uint32_t sampled_node_size, std::vector<std::string> las_input_paths) : futures(0), pool(32) {
	this->bounding_cube = bounding_cube;
	this->num_points = num_points;
	this->output_path = output_path;
	this->max_node_size = max_node_size;
	this->sampled_node_size = sampled_node_size;
	this->points_processed = 0;
	this->las_input_paths = las_input_paths;
	octree_file = 0;
	octree_file_cursor = 0;
	octree_file_path = get_octree_file(output_path);
}