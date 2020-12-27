#include "Builder.h"

void Builder::ic_load_points(Node* node) {
	FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
	if (!points_file) throw std::exception("Could not open file");

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
		if (!node->points.size()) throw std::exception("No points loaded");
		FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "wb");
		if (!points_file) throw std::exception("Could not open file");

		for (uint64_t i = 0; i < node->num_points; i++) {
			fwrite(&node->points[i], sizeof(struct Point), 1, points_file);
		}
		std::vector<Point>().swap(node->points); // Clear points and free memory

		fclose(points_file);
	}
}

void Builder::split_node(Node* node) {
	if (node->num_points > max_node_size) {
		if (node->num_points < 2'000'000) {
			// Split this node in-core
			ic_load_points(node);

			std::filesystem::remove(get_full_point_file(node->id, output_path));
			ic_split_node(node);
			return;
		}
		else if (node->num_points > 10'000'000) {
			// Run async
			futures.push_back(std::async(std::launch::async, [this, node] {
				split_node(node);
			}));
			return;
		}

		FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "rb");
		if (!points_file) throw std::exception("Could not open file");

		FILE* child_point_files[8] = { nullptr };
		uint64_t num_child_points[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		Point p;
		while (fread(&p, sizeof(struct Point), 1, points_file)) {
			uint8_t index = find_child_node_index(node->bounds, p);
			if (!child_point_files[index]) {
				child_point_files[index] = fopen(get_full_point_file(node->id + std::to_string(index), output_path).c_str(), "wb");
				if (!child_point_files[index]) throw std::exception("Could not open file");
			}
			fwrite(&p, sizeof(struct Point), 1, child_point_files[index]);
			num_child_points[index]++;
		}
		fclose(points_file);

		std::filesystem::remove(get_full_point_file(node->id, output_path));
		Logger::log_info("Removed " + node->id);
		node->num_points = 0;

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

				split_node(child_node);
			}
		}
	}
}

Node* Builder::build() {
	Node* root_node = new Node();
	root_node->id = "";
	root_node->bounds = bounding_cube;
	root_node->child_nodes_mask = 0;
	root_node->num_points = num_points;

	split_node(root_node);

	std::chrono::milliseconds wait_span(100);
	while (futures.size() > 0) {
		for (int i = 0; i < futures.size(); i++) {
			if (futures[i].wait_for(wait_span) == std::future_status::ready) {
				futures.erase(futures.begin() + i);
				i--;
			}
		}
	}

	return root_node;
}

Builder::Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
	uint32_t max_node_size) : futures(0) {
	this->bounding_cube = bounding_cube;
	this->num_points = num_points;
	this->output_path = output_path;
	this->max_node_size = max_node_size;
}