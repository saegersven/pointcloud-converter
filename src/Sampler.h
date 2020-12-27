#pragma once
#include <random>
#include <string>
#include <algorithm>
#include "Data.h"
#include "Utils.h"

void oc_sample_node(Node* node, uint64_t num_points, const std::string& output_path) {
	FILE* points_file = fopen(get_full_point_file(node->id, output_path).c_str(), "wb");

	FILE* child_node_files[8];
	uint64_t total_child_points = 0;
	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			child_node_files[i] = fopen(get_full_point_file(node->child_nodes[i]->id, output_path).c_str(), "rb");

			if (!child_node_files[i]) throw std::exception("Could not open file");
			total_child_points += node->child_nodes[i]->num_points;
		}
	}
	uint64_t to_sample = std::min(num_points, total_child_points);

	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			uint64_t to_sample_node = ((double)node->child_nodes[i]->num_points / (double)total_child_points) * to_sample;

			for (int j = 0; j < to_sample_node; j++) {
				// Copy point
				fseek(child_node_files[i], (node->child_nodes[i]->num_points / to_sample_node) * j * 12, SEEK_SET);
				Point p;
				fread(&p, sizeof(struct Point), 1, child_node_files[i]);
				fwrite(&p, sizeof(struct Point), 1, points_file);
			}
			fclose(child_node_files[i]);
		}
	}
	node->num_points = to_sample;
	fclose(points_file);
}

void sample_node(Node* node, uint64_t num_points, const std::string& output_path) {
	if (node->num_points == 0) {
		uint64_t total_child_points = 0;
		for (int i = 0; i < 8; i++) {
			if (node->child_nodes_mask & (1 << i)) {
				sample_node(node->child_nodes[i], num_points, output_path);
				total_child_points += node->child_nodes[i]->num_points;
			}
		}

		oc_sample_node(node, num_points, output_path);
	}
}