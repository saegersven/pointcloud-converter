#include <mutex>
#include <string>
#include <iostream>
#include <random>
#include <future>
#include "Utils.h"
#include "Data.h"
#include "Builder.h"
#include "BufferedPointReader.h"
#include "Logger.h"

void Builder::load_points(Node* node) {
	FILE* points_file = fopen(get_full_point_file(node->id, output_path, "").c_str(), "rb");
	node->points.reserve(node->num_points);

	Point p;
	for (uint64_t i = 0; i < node->num_points; i++) {
		if (!fread(&p, sizeof(struct Point), 1, points_file)) break;
		node->points.push_back(p);
	}

	fclose(points_file);
}

void Builder::split_node_in_core(Node* node) {
	if (node->num_points > max_node_size) {
		std::vector<Point> child_points[8];
		for (int i = 0; i < 8; i++) {
			child_points[i].reserve(node->num_points / 4);
		}
		for (uint64_t i = 0; i < node->num_points; i++) {
			uint8_t index = 0;
			index |= ((node->points[i].x > node->bounds.center_x ? 1 : 0) << 2);
			index |= ((node->points[i].y > node->bounds.center_y ? 1 : 0) << 1);
			index |= ((node->points[i].z > node->bounds.center_z ? 1 : 0) << 0);
			child_points[index].push_back(node->points[i]);
		}

		node->child_nodes = new Node * [8];
		for (int i = 0; i < 8; i++) {
			if (child_points[i].size() != 0) {
				Node* child_node = new Node();
				child_node->id = node->id;
				child_node->id.append(std::to_string(i));

				child_node->num_points = child_points[i].size();
				child_node->points = child_points[i];
				//memcpy(child_node->points, &child_points[i][0], child_points[i].size());

				child_node->bounds.center_x = node->bounds.center_x + (-(node->bounds.size / 2.0f) + ((i & (1 << 2)) ? node->bounds.size : 0));
				child_node->bounds.center_y = node->bounds.center_y + (-(node->bounds.size / 2.0f) + ((i & (1 << 1)) ? node->bounds.size : 0));
				child_node->bounds.center_z = node->bounds.center_z + (-(node->bounds.size / 2.0f) + ((i & (1 << 0)) ? node->bounds.size : 0));
				child_node->bounds.size = node->bounds.size / 2.0f;

				node->child_nodes_mask |= (1 << i);
				node->child_nodes[i] = child_node;

				split_node_in_core(child_node); // Try splitting up this node
			}
		}
		node->num_points = 0;
		std::vector<Point>().swap(node->points); // Deallocate memory
	}
	else {
		if (!node->points.size()) throw std::exception("No points loaded");
		FILE* points_file = fopen(get_full_point_file(node->id, output_path, "").c_str(), "wb");

		for (uint64_t i = 0; i < node->points.size(); i++) {
			fwrite(&node->points[i], sizeof(struct Point), 1, points_file);
		}

		std::vector<Point>().swap(node->points); // Deallocate memory

		fclose(points_file);
	}
}

/// <summary>
/// Recursively split nodes that have too many points to create an octree
/// </summary>
/// <param name="node">The root node of the tree</param>
void Builder::split_node(Node* node, bool is_async) {
	if (node->num_points > max_node_size) {
		if (node->num_points < 2'000'000) { // We can safely split this node in-core
			std::async(std::launch::async, [this](Node* node) {
				threads_finished_lock.lock();
				int index = threads_finished.size();
				threads_finished.push_back(false);
				threads_finished_lock.unlock();

				Logger::add_thread_alias(std::this_thread::get_id(), "SIC" + node->id);
				load_points(node);
				std::filesystem::remove(get_full_point_file(node->id, output_path, ""));
				split_node_in_core(node);
				Logger::log_info("Done splitting async");

				threads_finished_lock.lock();
				threads_finished[index] = true;
				threads_finished_lock.unlock();
			}, node);
			return;
		}
		/*else if (node->num_points < 4'000'000 && !is_async) {
			// Split this node asynchronously
			threads.push_back(std::async([this, is_async](Node* node) {
				Logger::add_thread_alias(std::this_thread::get_id(), "SUB" + node->id);
				split_node(node, true);
				Logger::log_info("Done splitting async");
			}, node));
			return;
		}*/

		uint64_t initial_file_size = std::filesystem::file_size(get_full_point_file(node->id, output_path, ""));
		uint64_t total_child_file_size = 0;

		FILE* points_file = nullptr;

		points_file = fopen(get_full_point_file(node->id, output_path, "").c_str(), "rb");

		if (!points_file) throw std::exception("Could not open file");

		FILE* child_point_files[8];
		std::string child_node_paths[8];
		for (int i = 0; i < 8; i++) {
			child_node_paths[i] = get_full_point_file(std::to_string(i), output_path, node->id);
			child_point_files[i] = fopen(child_node_paths[i].c_str(), "ab");

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

			total_child_file_size += std::filesystem::file_size(child_node_paths[i]);

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
				split_node(child_node, false);
			}
		}
		fclose(points_file);

		uint64_t lost_bytes = initial_file_size - total_child_file_size;
		if (lost_bytes != 0) std::cout << "Lost " << lost_bytes << "b (splitting node " << node->id << ")" << std::endl;

		// Delete the points file and set number of points for this node to 0
		node->num_points = 0;
		remove(get_full_point_file(node->id, output_path, "").c_str());
	} else {
		total_final_points += node->num_points;
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
	FILE* points_file = fopen(get_full_point_file(node->id, output_path, "").c_str(), "ab");
	if (!points_file)
		throw std::exception(("Failed to open file " + node->id).c_str());

	FILE* child_point_files[8] = {};
	std::string child_node_paths[8] = {};

	uint64_t total_child_points = 0;
	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			total_child_points += node->child_nodes[i]->num_points;

			child_node_paths[i] = get_full_point_file(std::to_string(i), output_path, node->id);
			child_point_files[i] = fopen(child_node_paths[i].c_str(), "rb");
			if(!child_point_files[i])
				throw std::exception(("Failed to open file " + get_full_point_file(std::to_string(i), "", node->id)).c_str());
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

	try {
		total_final_points = 0;
		split_node(node, false);

		while (threads_finished.size() == 0);

		bool all_finished = false;
		while (!all_finished) {
			all_finished = true;
			threads_finished_lock.lock();
			for (int i = 0; i < threads_finished.size(); i++) {
				if (threads_finished[i] == false) {
					all_finished = false;
					break;
				}
			}
			threads_finished_lock.unlock();
		}

		Logger::log_info("Split " + std::to_string(total_final_points) + "/" + std::to_string(num_points) + " points");

		sample_tree(node);
	}
	catch (std::exception e) {
		Logger::log_error("Error building:");
		Logger::log_error(e.what());

		return nullptr;
	}

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
