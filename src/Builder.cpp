#include "Builder.h"

/// <summary>
/// Recursively split nodes that have too many points to create an octree
/// </summary>
/// <param name="node">The root node of the tree</param>
void Builder::split_node(Node* node) {
	if (node->num_points > max_node_size) {
		FILE* points_file;
#ifdef _BUILD_CMAKE
		points_file = fopen(get_full_point_file(node->id, output_path, "").c_str(), "rb");
#else
		fopen_s(&points_file, get_full_point_file(node->id, output_path, "").c_str(), "rb");
#endif

		if (!points_file) throw std::exception("Could not open file");

		FILE* child_point_files[8];
		for (int i = 0; i < 8; i++) {
			std::string file_path = get_full_point_file(std::to_string(i), output_path, node->id);
#ifdef _BUILD_CMAKE
			child_point_files[i] = fopen(file_path.c_str(), "ab");
#else
			fopen_s(&child_point_files[i], file_path.c_str(), "ab");
#endif

			if (!child_point_files[i]) throw std::exception("Could not open file");
		}

		uint64_t num_child_points[8] = { 0 };
		uint64_t total_child_points = 0;
		// Read through all points of this node and sort them into the child nodes
		Point p;
		while (fread(&p, sizeof(p), 1, points_file)) {
			uint8_t index = 0;
			index |= ((p.x > node->bounds.center_x ? 1 : 0) << 2);
			index |= ((p.y > node->bounds.center_y ? 1 : 0) << 1);
			index |= ((p.z > node->bounds.center_z ? 1 : 0) << 0);

			fwrite(&p, sizeof(p), 1, child_point_files[index]);
			num_child_points[index]++;
			total_child_points++;
		}

		node->child_nodes = new Node * [8];
		for (int i = 0; i < 8; i++) {
			fclose(child_point_files[i]);

			if (num_child_points[i] != 0) {
				Node* child_node = new Node();
				child_node->id = node->id;
				child_node->id.append(std::to_string(i));
				child_node->num_points = num_child_points[i];
				child_node->bounds.center_x = node->bounds.center_x + (-(node->bounds.size / 2.0f) + ((i & (1 << 2)) ? node->bounds.size : 0));
				child_node->bounds.center_y = node->bounds.center_y + (-(node->bounds.size / 2.0f) + ((i & (1 << 1)) ? node->bounds.size : 0));
				child_node->bounds.center_z = node->bounds.center_z + (-(node->bounds.size / 2.0f) + ((i & (1 << 0)) ? node->bounds.size : 0));
				child_node->bounds.size = node->bounds.size / 2.0f;

				node->child_nodes_mask |= (1 << i);
				node->child_nodes[i] = child_node;

				// Try splitting up the node
				split_node(child_node);
			}
		}
		fclose(points_file);

		// Delete the points file and set number of points for this node to 0
		node->num_points = 0;
		remove(get_full_point_file(node->id, output_path, "").c_str());
	}
}

/// <summary>
/// Creates a sample of the child node's points in a node
/// </summary>
/// <param name="node">The node to be sampled</param>
/// <param name="num_points">The number of points to be sampled from the child nodes</param>
/// <param name="output_path">Output path, has to be passed because this method is static</param>
/// <param name="hierarchy_prefix">Prefix to be added before the node's hierarchy</param>
/// <returns>The actual number of points sampled. Can be smaller than num_points if the child nodes don't contain enough points</returns>
uint64_t Builder::sample(Node* node, uint32_t num_points, std::string output_path) {
	FILE* points_file;
	if (fopen_s(&points_file, get_full_point_file(node->id, output_path, "").c_str(), "ab") != NULL)
		throw std::exception("Failed to open file");

	FILE* child_point_files[8] = {};

	uint64_t total_child_points = 0;
	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			total_child_points += node->child_nodes[i]->num_points;

			std::string child_node_path = get_full_point_file(std::to_string(i), output_path, node->id);
			if(fopen_s(&child_point_files[i], child_node_path.c_str(), "rb") != NULL)
				throw std::exception("Failed to open file");
		}
	}

	uint64_t to_sample = total_child_points < num_points ? total_child_points : num_points;

	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i) && child_point_files[i]) {
			// Calculate how many points to sample from this node to properly distribute sources for the sampled points
			uint64_t sample_from_this_node = std::lround(((double)node->child_nodes[i]->num_points / (double)total_child_points) * (double)to_sample);

			std::vector<uint32_t> point_indices(sample_from_this_node);

			std::minstd_rand lce; // linear_congrential_engine
			for (int j = 0; j < sample_from_this_node; j++) {
				// Generate a random index that has not been generated before
				int point_index;
				bool is_unique = true;
				do {
					point_index = lce() % node->child_nodes[i]->num_points;

					is_unique = true;
					for (int k = 0; k < j; k++) {
						if (point_indices[k] == point_index) {
							is_unique = false;
							break;
						}
					}
				} while (!is_unique);

				point_indices[j] = point_index;

				// Read this random point from the child file
				fseek(child_point_files[i], point_index * 12, SEEK_SET);
				Point p;
				fread(&p, sizeof(p), 1, child_point_files[i]);
				
				// Copy this point into the parent node
				fwrite(&p, sizeof(p), 1, points_file);
			}

			fclose(child_point_files[i]);
		}
		node->num_points = to_sample;
	}
	fclose(points_file);

	return to_sample;
}

/// <summary>
/// Recursively sample nodes
/// </summary>
/// <param name="node">The root node of the tree</param>
void Builder::sample_tree(Node* node) {
	if (node->child_nodes_mask) {
		// This node has child nodes, which means it has no points yet and has to be sampled
		// But maybe the child nodes aren't sampled either, so check if any of them has no points
		for (int i = 0; i < 8; i++) {
			if (node->child_nodes_mask & (1 << i)) {
				// This child node exists
				if (node->child_nodes[i]->num_points == 0) {
					// Since this node does not have points, it has to have children and is not sampled yet
					sample_tree(node->child_nodes[i]);
				}
			}
		}
		// Now all the child nodes are sampled or are already at the lowest level, so we can sample this node
		Builder::sample(node, sampled_node_size, output_path);
	}
}

/// <summary>
/// Create an octree and sample the points
/// </summary>
/// <returns>A pointer to the root node of the tree</returns>
Node* Builder::build()
{
	Node* node = new Node();

	node->id = hierarchy_prefix;
	node->bounds = bounding_cube;
	node->child_nodes_mask = 0;
	node->num_points = num_points;

	split_node(node);

	sample_tree(node);

	return node;
}

Builder::Builder(Cube bounding_cube, uint64_t num_points, std::string output_path,
	std::string hierarchy_prefix, uint32_t max_node_size, uint32_t sampled_node_size) {
	this->bounding_cube = bounding_cube;
	this->num_points = num_points;
	this->output_path = output_path;
	this->hierarchy_prefix = hierarchy_prefix;
	this->max_node_size = max_node_size;
	this->sampled_node_size = sampled_node_size;
}
