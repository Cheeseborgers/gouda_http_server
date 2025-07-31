//
// Created by fason on 26/07/25.
//

#include "thread_pool.hpp"

#include <chrono>

#include "logger.hpp"

ThreadPool::ThreadPool(const size_t num_threads)
        : m_stop(false), m_pending_tasks(0) {
    for (size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(
            [this, thread_id = i](const std::stop_token& stoken) {
                worker_loop(stoken, thread_id);
            }
        );
    }
}

ThreadPool::~ThreadPool() {
    if (!m_stop) {
        stop();
    }
}

void ThreadPool::wait_for_all() {
    std::unique_lock lock(m_wait_mutex);
    m_wait_condition.wait(lock, [this] {
        return m_pending_tasks.load() == 0;
    });
}

void ThreadPool::stop() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_condition.notify_all();
    LOG_INFO("Thread pool stop requested");
}

// Private --------------------
void ThreadPool::worker_loop(std::stop_token stoken, const size_t thread_id) {
    const std::string thread_name = "Worker-" + std::to_string(thread_id);

    while (!stoken.stop_requested()) {
        std::unique_ptr<TaskBase> task;

        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, stoken, [this, &stoken] {
                return m_stop || !m_tasks.empty() || stoken.stop_requested();
            });

            if ((m_stop && m_tasks.empty()) || stoken.stop_requested())
                break;

            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }

        if (task) {
            auto start = std::chrono::steady_clock::now();
            LOG_DEBUG(std::format("[{}] Executing task...", thread_name));

            try {
                (*task)();
            } catch (const std::exception &e) {
                LOG_ERROR(std::format("[{}] Task threw exception: {}", thread_name, e.what()));
            }

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            LOG_DEBUG(std::format("[{}] Task completed in {}ms", thread_name, duration));

            if (--m_pending_tasks == 0) {
                std::lock_guard wait_lock(m_wait_mutex);
                m_wait_condition.notify_all();
            }
        }
    }

    LOG_INFO(std::format("[{}] Thread exiting", thread_name));
}

