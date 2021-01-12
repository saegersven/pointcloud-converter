#pragma once
#include <string>
#include "Data.h"

class PointReader {
protected:
	FILE* file;
	bool eof;

public:
	virtual void open(std::string filename) {};
	virtual Point read_point() { return Point(); };
	virtual bool has_points() { return false; }; // Has to be called after read_point

	~PointReader() {
		fclose(file);
	}
};