#include "Logger.h"
std::shared_ptr<Logger> Logger::_instance;

std::shared_ptr<Logger> Logger::instance() {
	if (_instance) return _instance;
	return _instance = std::make_shared<Logger>();
}

void Logger::log_info(std::string message) {
	instance()->log("INFO", message);
}

void Logger::log_warning(std::string message) {
	instance()->log("WARN", message);
}

void Logger::log_error(std::string message) {
	instance()->log("ERROR", message);
}

void Logger::log(std::string mode, std::string message) {
	std::string m = "";

	std::thread::id this_id = std::this_thread::get_id();
	if (thread_aliases.find(this_id) != thread_aliases.end()) {
		m.append("(" + thread_aliases[this_id] + ")");
	}

	m.append("[" + mode + "]:\t");
	m.append(message);

	lock.lock();
	std::cout << m << std::endl;
	lock.unlock();
}

void Logger::add_thread_alias(std::thread::id id, std::string alias) {
	instance()->thread_aliases.insert_or_assign(id, alias);
}