// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <deque>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "common/unique_function.h"

namespace Common {

template <class StateType = void>
class StatefulThreadWorker {
    static constexpr bool with_state = !std::is_same_v<StateType, void>;

    struct DummyCallable {
        int operator()() const noexcept {
            return 0;
        }
    };

    using Task =
        std::conditional_t<with_state, UniqueFunction<void, StateType*>, UniqueFunction<void>>;
    using StateMaker = std::conditional_t<with_state, std::function<StateType()>, DummyCallable>;

public:
    explicit StatefulThreadWorker(size_t num_workers, std::string name, StateMaker func = {},
                                  ThreadPriority priority = ThreadPriority::Normal,
                                  bool pin_to_physical_core_siblings = false,
                                  bool boost_priority_work = false)
        : workers_queued{num_workers}, thread_name{std::move(name)}, thread_priority{priority},
          use_physical_core_siblings{pin_to_physical_core_siblings},
          boost_priority_work_to_high{boost_priority_work} {
        const auto lambda = [this, func](std::stop_token stop_token, size_t worker_index) {
            Common::SetCurrentThreadName(thread_name.c_str());
            Common::SetCurrentThreadPriority(thread_priority);
            if (use_physical_core_siblings) {
                Common::PinCurrentThreadToPhysicalCoreSibling(worker_index);
                Common::SetCurrentThreadPowerThrottling(thread_priority == ThreadPriority::Low);
            }
            {
                [[maybe_unused]] std::conditional_t<with_state, StateType, int> state{func()};
                while (!stop_token.stop_requested()) {
                    Task task;
                    bool priority_task = false;
                    {
                        std::unique_lock lock{queue_mutex};
                        if (priority_requests.empty() && requests.empty()) {
                            wait_condition.notify_all();
                        }
                        condition.wait(lock, stop_token, [this] {
                            return !priority_requests.empty() || !requests.empty();
                        });
                        if (stop_token.stop_requested()) {
                            break;
                        }
                        if (!priority_requests.empty()) {
                            task = std::move(priority_requests.front());
                            priority_requests.pop_front();
                            priority_task = true;
                        } else {
                            task = std::move(requests.front());
                            requests.pop_front();
                        }
                    }
                    const bool should_boost_priority =
                        priority_task && boost_priority_work_to_high &&
                        static_cast<std::underlying_type_t<ThreadPriority>>(thread_priority) <
                            static_cast<std::underlying_type_t<ThreadPriority>>(
                                ThreadPriority::High);
                    if (should_boost_priority) {
                        Common::SetCurrentThreadPriority(ThreadPriority::High);
                        Common::SetCurrentThreadPowerThrottling(false);
                    }
                    if constexpr (with_state) {
                        task(&state);
                    } else {
                        task();
                    }
                    if (should_boost_priority) {
                        Common::SetCurrentThreadPriority(thread_priority);
                        if (use_physical_core_siblings) {
                            Common::SetCurrentThreadPowerThrottling(thread_priority ==
                                                                    ThreadPriority::Low);
                        }
                    }
                    ++work_done;
                    if (use_physical_core_siblings) {
                        std::this_thread::yield();
                    }
                }
            }
            ++workers_stopped;
            wait_condition.notify_all();
        };
        threads.reserve(num_workers);
        for (size_t i = 0; i < num_workers; ++i) {
            threads.emplace_back(lambda, i);
        }
    }

    StatefulThreadWorker& operator=(const StatefulThreadWorker&) = delete;
    StatefulThreadWorker(const StatefulThreadWorker&) = delete;

    StatefulThreadWorker& operator=(StatefulThreadWorker&&) = delete;
    StatefulThreadWorker(StatefulThreadWorker&&) = delete;

    void QueueWork(Task work) {
        QueueWorkImpl(std::move(work), false);
    }

    void QueuePriorityWork(Task work) {
        QueueWorkImpl(std::move(work), true);
    }

    void WaitForRequests(std::stop_token stop_token = {}) {
        std::stop_callback callback(stop_token, [this] {
            for (auto& thread : threads) {
                thread.request_stop();
            }
        });
        std::unique_lock lock{queue_mutex};
        wait_condition.wait(lock, [this] {
            return workers_stopped >= workers_queued || work_done >= work_scheduled;
        });
    }

private:
    void QueueWorkImpl(Task work, bool priority) {
        {
            std::unique_lock lock{queue_mutex};
            if (priority) {
                priority_requests.emplace_back(std::move(work));
            } else {
                requests.emplace_back(std::move(work));
            }
            ++work_scheduled;
        }
        condition.notify_one();
    }

    std::deque<Task> priority_requests;
    std::deque<Task> requests;
    std::mutex queue_mutex;
    std::condition_variable_any condition;
    std::condition_variable wait_condition;
    std::atomic<size_t> work_scheduled{};
    std::atomic<size_t> work_done{};
    std::atomic<size_t> workers_stopped{};
    std::atomic<size_t> workers_queued{};
    std::string thread_name;
    ThreadPriority thread_priority;
    bool use_physical_core_siblings;
    bool boost_priority_work_to_high;
    std::vector<std::jthread> threads;
};

using ThreadWorker = StatefulThreadWorker<>;

} // namespace Common
