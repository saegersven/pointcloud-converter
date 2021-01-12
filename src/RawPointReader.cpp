#include "RawPointReader.h"
#include <stdexcept>

void RawPointReader::open(std::string filename) {
	file = fopen(filename.c_str(), "rb");
	if (!file) throw std::runtime_error("Could not open file");
}

bool RawPointReader::has_points() {
	return eof;
}

Point RawPointReader::read_point() {
	Point p;
	eof = fread(&p, sizeof(p), 1, file);
	return p;
}