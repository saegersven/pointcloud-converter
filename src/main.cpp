#include <string>
#include <filesystem>
#include "Logger.h"
#include "Reader.h"
#include "Builder.h"
#include "Utils.h"
#include "HierarchyWriter.h"

//#define SKIP_READ
#define SKIP_BOUNDS { 372.735f, 36.274f, 568.365f, 134.426f }

enum class ErrCode {
	INVALID_ARGS = 1,
	OUT_NOT_EMPTY = 2,
	BUILD_FAIL = 3,
	SAMPLE_FAIL = 4,
};

void fail(ErrCode code) {
	Logger::log_error("Conversion failed (error code " + std::to_string((int)code) + ")");
	exit((int)code);
}

int main(int argc, char* argv[]) {
	Logger::add_thread_alias("MAIN");

	if (argc != 3) {
		Logger::log_error("Invalid arguments");
		fail(ErrCode::INVALID_ARGS);
	}

	bool is_dir = std::filesystem::is_directory(argv[1]);
	std::vector<std::string> input_files;
	if (is_dir) {
		const std::string ext = ".las";
		// Iterate through all files in directory
		for (auto& p : std::filesystem::recursive_directory_iterator(argv[1])) {
			if (p.path().extension() == ext) input_files.push_back(p.path().string());
		}
		if (input_files.size() == 0) {
			Logger::log_error("No input files in directory");
			fail(ErrCode::INVALID_ARGS);
		}
	}
	else {
		if (!check_file(argv[1])) {
			Logger::log_error("Could not open input file");
			fail(ErrCode::INVALID_ARGS);
		}
		input_files.push_back(argv[1]);
	}

	std::filesystem::create_directories(argv[2]);

	if (!is_directory_empty(argv[2])) {
		Logger::log_error("Output directory must be empty");
		fail(ErrCode::OUT_NOT_EMPTY);
	}

	auto start_time = std::chrono::high_resolution_clock::now();

	Logger::log_info("Reading points");

	//Reader r(input_files, argv[2]);

	Cube bounding_cube;
	uint64_t num_points = 0;
	Reader::read_bounds_las(input_files, bounding_cube, num_points);

	Logger::log_info("Done reading");

	Logger::log_info("Bounds: " + bounding_cube.to_string());

	Builder b(bounding_cube, num_points, argv[2], 30'000, 30'000, input_files);

	Logger::log_info("Building octree");
	auto sub_start_time = std::chrono::high_resolution_clock::now();

	Node* root_node;
	try {
		root_node = b.build();
	}
	catch (std::exception e) {
		Logger::log_error("Error building:");
		Logger::log_error(e.what());
		std::cin.get();
		fail(ErrCode::BUILD_FAIL);
		return 0;
	}

	uint64_t sub_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - sub_start_time).count();
	Logger::log_info("Building took " + std::to_string(sub_time) + "ms");

	Logger::log_info("Writing hierarchy");
	write_hierarchy(root_node, std::string(argv[2]) + "/hierarchy.bin");

	delete root_node;

	sub_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
	Logger::log_info("Total time: " + std::to_string(sub_time) + "ms");

	std::cin.get();
}