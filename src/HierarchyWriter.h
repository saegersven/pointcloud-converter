#pragma once
#include <stdexcept>
#include "Data.h"

void write_node_hierarchy(Node* node, FILE* file, bool write_bounds) {
	if (write_bounds) {
		fwrite(&node->bounds, sizeof(node->bounds), 1, file);
	}
	fwrite(&node->num_points, sizeof(node->num_points), 1, file);
	fwrite(&node->child_nodes_mask, sizeof(node->child_nodes_mask), 1, file);

	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			write_node_hierarchy(node->child_nodes[i], file, false);
		}
	}
}

void write_hierarchy(Node* root_node, const std::string& path) {
	FILE* hierarchy_file = fopen(path.c_str(), "wb");

	if (!hierarchy_file) throw std::runtime_error("Could not open hierarchy file");

	write_node_hierarchy(root_node, hierarchy_file, true);

	fclose(hierarchy_file);
}