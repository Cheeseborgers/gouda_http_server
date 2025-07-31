#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>
#include <atomic>

// Base type-erased task wrapper
struct TaskBase {
    virtual ~TaskBase() = default;
    virtual void operator()() = 0;
};

template <typename F>
struct TaskImpl final : TaskBase {
    F func;
    explicit TaskImpl(F &&f) : func(std::move(f)) {}
    void operator()() override { func(); }
};

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);

    ~ThreadPool();

    template <typename F>
    void enqueue(F &&f) {
        {
            std::lock_guard lock(m_mutex);
            m_tasks.push(std::make_unique<TaskImpl<std::decay_t<F>>>(std::forward<F>(f)));
            ++m_pending_tasks;
        }
        m_condition.notify_one();
    }

    // Wait until all current tasks finish
    void wait_for_all();
    void stop();

private:
    void worker_loop(std::stop_token stoken, size_t thread_id);

private:
    std::vector<std::jthread> m_workers;
    std::queue<std::unique_ptr<TaskBase>> m_tasks;

    std::mutex m_mutex;
    std::condition_variable_any m_condition;

    std::atomic<bool> m_stop;
    std::atomic<size_t> m_pending_tasks;

    std::mutex m_wait_mutex;
    std::condition_variable m_wait_condition;
};

#endif // THREAD_POOL_H
