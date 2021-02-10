#pragma once
#include <vector>
#include <future>
#include <mutex>

class ThreadPool
{
private:
	std::vector<std::shared_future<void>> threads;
	std::mutex jobs_lock;
	std::vector<std::function<void()>> jobs;
	std::atomic<bool> done;

public:
	ThreadPool(const uint16_t num_threads);
	void add_job(std::function<void()> job);
	void wait();
};

