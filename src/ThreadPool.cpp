#include "ThreadPool.h"
#include "Logger.h"
#include <string>

void ThreadPool::spawn(const uint16_t id) {
	threads[id] = std::async(std::launch::async, [this] {
		try {
			while (true) {
				jobs_lock.lock();

				if (jobs.empty()) {
					jobs_lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					jobs_lock.lock();
					bool empty = jobs.size() == 0;
					jobs_lock.unlock();
					if (empty) break;
					continue;
				}

				std::function<void()> job = jobs.front();
				jobs.pop();
				jobs_lock.unlock();

				job();
			}
		}
		catch (std::exception exc) {
			Logger::log_error("Error in thread: " + std::string(exc.what()));
		}
	});
}

ThreadPool::ThreadPool(const uint16_t num_threads) {
	threads.resize(num_threads);
	for (uint16_t i = 0; i < num_threads; i++) {
		spawn(i);
	}
}

void ThreadPool::add_job(std::function<void()> job) {
	jobs_lock.lock();
	// Some threads may have stopped, wake them up
	for (int i = 0; i < threads.size(); i++) {
		if (threads[i].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			spawn(i);
			break; // No need to spawn more than one
		}
	}
	
	jobs.push(job);
	jobs_lock.unlock();
}

void ThreadPool::wait() {
	for (uint16_t i = 0; i < threads.size(); i++) {
		threads[i].wait();
	}
}

uint64_t ThreadPool::num_jobs() {
	return jobs.size();
}