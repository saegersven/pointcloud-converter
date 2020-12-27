#include <string>
#include <filesystem>
#include "Logger.h"
#include "Reader.h"
#include "Builder.h"
#include "Sampler.h"
#include "Utils.h"

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

	if (argc != 3 || !check_file(argv[1])) {
		Logger::log_error("Invalid arguments");
		fail(ErrCode::INVALID_ARGS);
	}

	std::filesystem::create_directories(argv[2]);

#ifndef SKIP_READ
	if (!is_directory_empty(argv[2])) {
		Logger::log_error("Output directory must be empty");
		fail(ErrCode::OUT_NOT_EMPTY);
	}
#endif

	auto start_time = std::chrono::high_resolution_clock::now();

	Logger::log_info("Reading points");

	Reader r(argv[1], argv[2]);

#ifdef SKIP_READ
	Cube bounding_cube = SKIP_BOUNDS;
	Logger::log_info("Skipping read");
#else
	Cube bounding_cube = r.read_bounds();
#endif

	Logger::log_info("Done reading");

	Logger::log_info("Bounds: " + bounding_cube.to_string());

	// Number of points can be retrieved from the file size of the root node points file (12 bytes per point)
	uint64_t num_points = std::filesystem::file_size(get_full_point_file("", argv[2])) / 12;
	Builder b(bounding_cube, num_points, argv[2], 15'000);

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

	Logger::log_info("Sampling");
	sub_start_time = std::chrono::high_resolution_clock::now();

	try {
		sample_node(root_node, 15'000, argv[2]);
	}
	catch (std::exception e) {
		Logger::log_error("Error sampling:");
		Logger::log_error(e.what());
		fail(ErrCode::SAMPLE_FAIL);
		return 0;
	}

	sub_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - sub_start_time).count();
	Logger::log_info("Sampling took " + std::to_string(sub_time) + "ms");

	delete root_node;

	sub_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
	Logger::log_info("Total time: " + std::to_string(sub_time) + "ms");

	std::cin.get();
}