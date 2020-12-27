#include "Logger.h"

std::shared_ptr<Logger> Logger::_instance;

std::shared_ptr<Logger> Logger::instance() {
	if (_instance) return _instance;
	return _instance = std::make_shared<Logger>();
}

void Logger::log_info(const std::string& message) {
	instance()->log("INFO", message);
}

void Logger::log_warning(const std::string& message) {
	instance()->log("WARN", message);
}

void Logger::log_error(const std::string& message) {
	instance()->log("ERROR", message);
}

void Logger::log_return(const std::string& message) {
	instance()->log("INFO", message, '\r');
}

void Logger::log_raw(const std::string& message) {
	instance()->log_raw_i(message);
}

void Logger::log_raw_i(const std::string& message) {
	lock.lock();
	std::cout << message;
	lock.unlock();
}

void Logger::log(const std::string& mode, const std::string& message) {
	log(mode, message, '\n');
}

void Logger::log(const std::string& mode, const std::string& message, const char line_ending) {
	std::string m = "";

	std::thread::id this_id = std::this_thread::get_id();
	if (thread_aliases.find(this_id) != thread_aliases.end()) {
		m.append("(" + thread_aliases[this_id] + ")");
	}

	m.append("[" + mode + "]:\t");
	m.append(message);

	lock.lock();
	std::cout << m << line_ending;
	lock.unlock();
}

void Logger::add_thread_alias(const std::string& alias) {
	add_thread_alias(std::this_thread::get_id(), alias);
}

void Logger::add_thread_alias(std::thread::id id, const std::string& alias) {
	instance()->thread_aliases.insert_or_assign(id, alias);
}