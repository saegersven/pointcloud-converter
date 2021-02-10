#include "ThreadPool.h"
#include "Logger.h"
#include <string>

ThreadPool::ThreadPool(const uint16_t num_threads) {
	threads.resize(num_threads);
	for (uint16_t i = 0; i < num_threads; i++) {
		const std::chrono::milliseconds sleep_time(10);
		threads[i] = std::async(std::launch::async, [this, sleep_time] {
			try {
				while (true) {
					jobs_lock.lock();

					if (jobs.size() == 0) {
						jobs_lock.unlock();
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
						continue;
					}

					std::function<void()> job = jobs[0];
					jobs.erase(jobs.begin() + 0);
					Logger::log_info(std::to_string(jobs.size()));
					jobs_lock.unlock();
					
					job();
				}
			}
			catch (std::exception exc) {
				Logger::log_error("Error in thread: " + std::string(exc.what()));
			}
		});
	}
}

void ThreadPool::add_job(std::function<void()> job) {
	jobs_lock.lock();
	jobs.push_back(job);
	jobs_lock.unlock();
}

void ThreadPool::wait() {
	for (uint16_t i = 0; i < threads.size(); i++) {
		threads[i].wait();
	}
}