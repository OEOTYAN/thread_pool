#pragma once

#include <atomic>
#include <functional>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <vector>

#include "concurrentqueue.h"

namespace oeo {

struct default_thread_init {
    constexpr void operator()(auto&&) const {}
};

struct default_task_invoke {
    template <class F>
    constexpr void operator()(F&& f) const {
        std::invoke(std::forward<F>(f));
    }
};

template <class Task = std::function<void()>, class Init = default_thread_init, class Invoke = default_task_invoke>
class thread_pool {
    using queue_type = moodycamel::ConcurrentQueue<Task>;
    using token_type = typename queue_type::consumer_token_t;

    std::vector<std::thread>  workers;
    queue_type                tasks;
    std::counting_semaphore<> semaphore{0};
    std::atomic_bool          stop{false};
    Init                      initer;
    Invoke                    invoker;

public:
    thread_pool(size_t threads)
        requires(std::is_default_constructible_v<Init> && std::is_default_constructible_v<Invoke>)
    : thread_pool(threads, Init{}, Invoke{}) {}

    thread_pool(size_t threads, Init init)
        requires(std::is_default_constructible_v<Invoke>)
    : thread_pool(threads, std::move(init), Invoke{}) {}

    thread_pool(size_t threads, Init init, Invoke invoke) : initer(std::move(init)), invoker(std::move(invoke)) {
        workers.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this, i] {
                initer(i);
                token_type token{tasks};
                for (;;) {
                    Task task;
                    semaphore.acquire();
                    while (!tasks.try_dequeue(token, task)) {
                        if (stop.load(std::memory_order_relaxed)) {
                            return;
                        }
                    }
                    invoker(task);
                }
            });
        }
    }

    void enqueue(Task&& task) {
        tasks.enqueue(std::move(task));
        semaphore.release();
    }

    ~thread_pool() {
        stop = true;
        semaphore.release(workers.size());
        for (auto& worker : workers) {
            if (worker.joinable())
                worker.join();
        }
    }
};
} // namespace oeo
