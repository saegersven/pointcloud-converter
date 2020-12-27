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

void Builder::ic_sample_node(Node* node) {
	FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
	if (!points_file) throw std::runtime_error("Could not open file");

	uint64_t sample_interval = node->num_points / sampled_node_size;
	for (uint64_t i = 0; i < sampled_node_size; i++) {
		fwrite(&node->points[i * sample_interval], sizeof(struct Point), 1, points_file);
	}

	fclose(points_file);
}

void Builder::ic_split_node(Node* node) {
	if (node->num_points > max_node_size) {
		std::vector<Point> child_points[8];
		for (int i = 0; i < 8; i++) {
			child_points[i].reserve(node->num_points / 4);
		}

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

		std::vector<Point>().swap(node->points); // Clear points from memory before attempting to split child nodes
		node->num_points = 0;

		for (int i = 0; i < 8; i++) {
			if (node->child_nodes_mask & (1 << i)) {
				ic_split_node(node->child_nodes[i]);
			}
		}
	}
	else {
		if (!node->points.size()) throw std::runtime_error("No points loaded");
		FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "wb");
		if (!points_file) throw std::runtime_error("Could not open file");

		for (uint64_t i = 0; i < node->num_points; i++) {
			fwrite(&node->points[i], sizeof(struct Point), 1, points_file);
		}
		std::vector<Point>().swap(node->points); // Clear points and free memory

		fclose(points_file);
		points_processed += node->num_points;
	}
}

void Builder::split_node(Node* node, bool is_async) {
	if (node->num_points > max_node_size) {
		if (node->num_points < 2'000'000) {
			if (num_points_in_core < 38'000'000) { // Ensure that only 40 million points are in memory at the same time
				uint64_t num_points_to_load = node->num_points;

				num_points_in_core += num_points_to_load;
				// Split this node in-core
				ic_load_points(node);

				std::filesystem::remove(get_full_point_file(node->id, output_path));

				ic_split_node(node);
				num_points_in_core -= num_points_to_load;
				return;
			}
		}
		else if (node->num_points > 5'000'000 && !is_async) {
			// Run async
			futures.push_back(std::async(std::launch::async, [this, node] {
				Logger::add_thread_alias("BLDA");
				split_node(node, true);
			}));
			return;
		}


		FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
		if (!points_file) throw std::runtime_error("Could not open file");

		// Since we will split this node, we can sample it now
		FILE* sample_file = fopen(get_full_temp_point_file(node->id, output_path).c_str(), "wb");
		if (!sample_file) throw std::runtime_error("Could not open file");

		FILE* child_point_files[8] = { nullptr };
		uint64_t num_child_points[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		uint64_t i = 0;
		uint64_t sample_interval = (int)((double)node->num_points / (double)sampled_node_size);

		Point p;
		while (fread(&p, sizeof(struct Point), 1, points_file)) {
			if (i % sample_interval == 0) fwrite(&p, sizeof(struct Point), 1, sample_file);

			uint8_t index = find_child_node_index(node->bounds, p);
			if (!child_point_files[index]) {
				child_point_files[index] = fopen(get_full_point_file(node->id + std::to_string(index), output_path).c_str(), "wb");
				if (!child_point_files[index]) throw std::runtime_error("Could not open file");
			}
			fwrite(&p, sizeof(struct Point), 1, child_point_files[index]);
			num_child_points[index]++;
			i++;
		}
		fclose(points_file);
		fclose(sample_file);

		std::filesystem::remove(get_full_point_file(node->id, output_path));
		node->num_points = 0;

		// Replace the file that contains all points with the temp file that contains the sampled subset
		std::filesystem::rename(get_full_temp_point_file(node->id, output_path), get_full_point_file(node->id, output_path));

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
	}
	else {
		points_processed += node->num_points;
	}
}

Node* Builder::build() {
	Node* root_node = new Node();
	root_node->id = "";
	root_node->bounds = bounding_cube;
	root_node->child_nodes_mask = 0;
	root_node->num_points = num_points;

	std::thread status_thread([this, root_node] {
		Logger::add_thread_alias("BUILD");
		std::chrono::milliseconds wait_for(3000);
		uint64_t total_points = root_node->num_points;
		while (points_processed < total_points) {
			uint64_t points = points_processed.load();
			Logger::log_return(std::to_string((int)((double)points / (double)total_points * 100.0)) + "% ("
				+ std::to_string(points) + "/" + std::to_string(total_points) + ")\r");
			std::this_thread::sleep_for(wait_for);
		}
	});
	status_thread.detach();

	split_node(root_node, true /*Don't make the root node async*/);

	std::chrono::milliseconds wait_span(500);
	while (futures.size() > 0) {
		for (int i = 0; i < futures.size(); i++) {
			if (futures[i].wait_for(wait_span) != std::future_status::ready) {
				continue;
			}
			futures[i].get();
			futures.erase(futures.begin() + i);
			i--;
		}
	}
	Logger::log_info("Done building                   ");

	return root_node;
}

Builder::Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
	uint32_t max_node_size, uint32_t sampled_node_size) : futures(0) {
	this->bounding_cube = bounding_cube;
	this->num_points = num_points;
	this->output_path = output_path;
	this->max_node_size = max_node_size;
	this->sampled_node_size = sampled_node_size;
	this->num_points_in_core = 0;
	this->points_processed = 0;
}