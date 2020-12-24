#include <iostream>
#include <chrono>
#include <future>
#include "Data.h"
#include "Reader.h"
#include "Builder.h"
#include "utils.h"

#define ERR_CODE_INVALID_ARGS 1
#define ERR_CODE_OUT_NOT_EMPTY 2
#define ERR_CODE_BUILDER_THREAD 3
#define ERR_CODE_HIERARCHY 4

#define MAX_NODE_SIZE 4096
#define SAMPLED_NODE_SIZE MAX_NODE_SIZE

void fail(int code) {
	std::cout << std::endl << "Conversion failed with code " << code << std::endl;
	std::cout << "Press any key to exit...";
	std::cin.get();
	exit(code);
}

void write_node_to_hierarchy(Node* node, FILE* file, bool write_bounds) {
	// FORMAT:
	// for root node:
	// center x: float
	// center y: float
	// center z: float
	// size: float
	// for all nodes:
	// number of points: 64 bit unsigned int
	// child node mask: 8 bit bitmask
	// + Child nodes recursively

	// id will be calculated at runtime
	// Bounds will be calculated at runtime for every node except the root node
	if (write_bounds) {
		fwrite(&node->bounds.center_x, sizeof(float), 1, file);
		fwrite(&node->bounds.center_y, sizeof(float), 1, file);
		fwrite(&node->bounds.center_z, sizeof(float), 1, file);
		fwrite(&node->bounds.size, sizeof(float), 1, file);
	}
	// Write number of points (64 bit int)
	fwrite(&node->num_points, sizeof(uint64_t), 1, file);
	// Write child mask (8 bit)
	fwrite(&node->child_nodes_mask, sizeof(uint8_t), 1, file);

	// Now write all the child nodes from first to last, if they exist
	for (int i = 0; i < 8; i++) {
		if (node->child_nodes_mask & (1 << i)) {
			write_node_to_hierarchy(node->child_nodes[i], file, false);
		}
	}
}

void write_hierarchy(Node* root_node, std::string path) {
	FILE* hierarchy_file;

	if (fopen_s(&hierarchy_file, path.c_str(), "wb") != NULL || !hierarchy_file) {
		fail(ERR_CODE_HIERARCHY);
		return;
	}

	write_node_to_hierarchy(root_node, hierarchy_file, true);

	fclose(hierarchy_file);
}

int main(int argc, char** argv)
{
	if (argc != 3 || !check_file(argv[1])) {
		std::cout << "Invalid arguments" << std::endl;
		fail(ERR_CODE_INVALID_ARGS);
	}

	// Create output directory
	std::filesystem::create_directories(argv[2]);
	// Check if an existing output directory is empty
	if (!is_directory_empty(argv[2])) {
		std::cout << "Output directory must be empty." << std::endl;
		fail(ERR_CODE_OUT_NOT_EMPTY);
	}

	auto start = std::chrono::high_resolution_clock::now();

	std::cout << "Reading... ";

	Reader r(argv[1], argv[2]);
	Cube bounding_cube = r.read_bounds();
	
	std::cout << "Done." << std::endl << std::endl;

	std::cout << "AABB:" << std::endl;
	std::cout << "Center\t" << "(" << bounding_cube.center_x << ", " << bounding_cube.center_y << ", " << bounding_cube.center_z << ")" << std::endl;
	std::cout << "Size\t" << bounding_cube.size << std::endl << std::endl;

	std::cout << "Distributing... ";
	// Split points into eight smaller sets of points
	SplitPointsMetadata splitPointsMetadata = r.split_points(bounding_cube);

	std::cout << "Done." << std::endl << std::endl << "Building octree..." << std::endl;

	std::vector<std::future<Node*>> future_trees(8);

	uint8_t threads_finished = 0;

	// Run 8 builders
	for (int i = 0; i < 8; i++) {
		if (splitPointsMetadata.num_points[i] == 0) {
			// Mark this thread as finished from the beginning
			threads_finished |= (1 << i);
			continue;
		}
		std::string hierarchy_prefix = std::to_string(i);
		Builder b(splitPointsMetadata.bounding_cubes[i], splitPointsMetadata.num_points[i], argv[2],
			hierarchy_prefix, MAX_NODE_SIZE, SAMPLED_NODE_SIZE);
		// Start building
		future_trees[i] = std::async(std::launch::async, [](Builder b) {
			return b.build();
		}, b);
	}

	std::chrono::milliseconds wait_span(100);
	while (threads_finished != 0xFF) {
		for (int i = 0; i < 8; i++) {
			if (splitPointsMetadata.num_points[i] == 0 || threads_finished & (1 << i)) continue;

			if (future_trees[i].wait_for(wait_span) == std::future_status::ready) {
				std::cout << "Thread " << i << " finished." << std::endl;
				std::cout << "Took "
					<< std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count()
					<< "ms" << std::endl << std::endl;
				threads_finished |= (1 << i);
			}
		}
	}

	std::cout << "Merging trees... ";

	Node* root_node = new Node();
	root_node->child_nodes = new Node*[8];
	for (int i = 0; i < 8; i++) {
		Node* child_node;
		try {
			child_node = future_trees[i].get();
		}
		catch (std::exception e) {
			std::cout << std::endl << "Could not fetch results of thread " << i << std::endl;
			delete root_node;
			fail(ERR_CODE_BUILDER_THREAD);
			return 0;
		}

		root_node->child_nodes_mask |= ((splitPointsMetadata.num_points[i] == 0 ? 0 : 1) << i);
		root_node->child_nodes[i] = child_node;
	}
	root_node->bounds = bounding_cube;
	root_node->id = "";
	Builder::sample(root_node, SAMPLED_NODE_SIZE, argv[2]);

	std::cout << "Done" << std::endl;

	std::cout << "Writing hierarchy... ";
	
	write_hierarchy(root_node, std::string(argv[2]) + "/hierarchy.bin");

	std::cout << "Done." << std::endl << std::endl;

	std::cout << "Total time: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count()
		<< "ms" << std::endl;

	delete root_node;

	std::cin.get();
}