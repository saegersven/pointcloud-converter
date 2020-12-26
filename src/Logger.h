#include <iostream>
#include <map>
#include <thread>
#include <mutex>
#include <memory>

class Logger {
public:
	static void log_info(std::string message);
	static void log_warning(std::string message);
	static void log_error(std::string message);

	static void add_thread_alias(std::thread::id id, std::string alias);

private:
	static std::shared_ptr<Logger> _instance;
	static std::shared_ptr<Logger> instance();

	std::map<std::thread::id, std::string> thread_aliases;
	std::mutex lock;

	void log(std::string mode, std::string message);
};