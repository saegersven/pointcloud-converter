#pragma once
#include <iostream>
#include <map>
#include <thread>
#include <mutex>
#include <memory>

class Logger {
public:
	static void log_info(const std::string& message);
	static void log_warning(const std::string& message);
	static void log_error(const std::string& message);

	static void add_thread_alias(const std::string& alias);
	static void add_thread_alias(std::thread::id id, const std::string& alias);

private:
	static std::shared_ptr<Logger> _instance;
	static std::shared_ptr<Logger> instance();

	std::map<std::thread::id, std::string> thread_aliases;
	std::mutex lock;

	void log(const std::string& mode, const std::string& message);
};