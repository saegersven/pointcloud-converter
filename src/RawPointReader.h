#pragma once
#include "PointReader.h"

class RawPointReader : public PointReader {
	void open(std::string filename) override;
	bool has_points() override;
	Point read_point() override;
};