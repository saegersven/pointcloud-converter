#include "Utils.h"

/// <summary>
/// Check if a directory is empty.
/// </summary>
/// <param name="path">The path to the directory</param>
/// <returns>true if the directory is empty, false if it is not or an error occured.</returns>
bool is_directory_empty(const std::string& path) {
	try {
		return std::filesystem::is_empty(path);
	}
	catch (std::exception) {
		return false;
	}
}

/// <summary>
/// Check if a file can be opened (if it exists)
/// </summary>
/// <param name="filename">The path to the file.</param>
/// <returns>true if the file can be opened, false otherwise.</returns>
bool check_file(const std::string& filename) {
	FILE* file;
	if (fopen_s(&file, filename.c_str(), "r") == NULL && file) {
		fclose(file);
		return true;
	}
	else {
		return false;
	}
}

/// <summary>
/// Get the path to the directory a file is in.
/// </summary>
/// <param name="file">The path to the file.</param>
/// <returns>The path to the directory.</returns>
std::string get_directory(std::string file) {
	size_t last_slash_index = file.rfind('\\');
	if (std::string::npos != last_slash_index) {
		return file.substr(0, last_slash_index);
	}
	else {
		last_slash_index = file.rfind('/');
		if (std::string::npos != last_slash_index) {
			return file.substr(0, last_slash_index);
		}
	}
}

/// <summary>
/// Convert a node hierarchy and a path into a full path for a specific point file.
/// </summary>
/// <param name="hierarchy">The hierarchy of the node.</param>
/// <param name="output_path">The output folder.</param>
/// <param name="hierarchy_prefix">A prefix to be added in front of the hierarchy.</param>
/// <returns>The path to the point file.</returns>
std::string get_full_point_file(std::string hierarchy, std::string output_path, std::string hierarchy_prefix)
{
	return output_path + "/p" + hierarchy_prefix + hierarchy;
}