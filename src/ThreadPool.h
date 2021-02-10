#pragma once
#include <queue>
#include <future>
#include <mutex>

class ThreadPool
{
private:
	std::vector<std::shared_future<void>> threads;
	std::mutex jobs_lock;
	std::queue<std::function<void()>> jobs;
	std::atomic<bool> done;

	void spawn(const uint16_t id);

public:
	ThreadPool(const uint16_t num_threads);
	void add_job(std::function<void()> job);
	void wait();
	uint64_t num_jobs();
};

