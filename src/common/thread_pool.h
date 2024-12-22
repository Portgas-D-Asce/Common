//
// Created by pk on 24-12-15.
//

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <vector>
#include <iostream>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

#include "singleton.h"


class ThreadPool {
public:
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    friend std::default_delete<ThreadPool>;
    friend Singleton<ThreadPool>;
private:
    explicit ThreadPool(size_t cnt = std::thread::hardware_concurrency());
    ~ThreadPool();
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // need to keep track of threads, so we can join them
    std::vector<std::thread> _workers;
    // the task queue
    std::queue<std::function<void()>> _tasks;
    // synchronization
    mutable std::mutex _mtx;
    mutable std::condition_variable _cv;
    bool _stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t cnt) : _stop(false) {
    for(size_t i = 0; i < cnt ; ++i) {
        _workers.emplace_back([this] {
            std::function<void()> task;
            for(;;) {
                {
                    std::unique_lock<std::mutex> lock(_mtx);
                    // stop == false && task.empty() -> 阻塞
                    _cv.wait(lock, [this] { return this->_stop || !this->_tasks.empty(); });

                    // stop == true && task.empty() -> 结束线程
                    if (_stop && _tasks.empty()) { return; }

                    // !task.empty() -> 继续运行
                    task = std::move(_tasks.front());
                    _tasks.pop();
                }
                task();
            }
        });
    }
}

// add new work item to the pool
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(_mtx);

        // don't allow enqueueing after stopping the pool
        if(_stop) throw std::runtime_error("enqueue on stopped ThreadPool");

        _tasks.emplace([task] () { (*task)(); });
    }

    _cv.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _stop = true;
    }
    _cv.notify_all();
    for(std::thread &worker: _workers) worker.join();
}

using SingletonThreadPool = Singleton<ThreadPool>;
#endif //THREAD_POOL_H
