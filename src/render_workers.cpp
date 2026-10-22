#include "render_workers.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace a2 {
namespace {
class Workers {
public:
  Workers() {
    unsigned count = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
    try {
      for (unsigned i = 1; i < count; ++i)
        threads_.emplace_back([this] { run(); });
    } catch (...) {
      // Already created workers remain usable when the host limits threads.
    }
  }
  ~Workers() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    wake_.notify_all();
    for (auto &thread : threads_)
      thread.join();
  }
  void rows(int rows, int band, const std::function<void(int, int)> &job) {
    std::lock_guard serial(serial_);
    {
      std::lock_guard lock(mutex_);
      job_ = job;
      rows_ = rows;
      band_ = band;
      next_ = 0;
      remaining_ = int(threads_.size());
      ++generation_;
    }
    wake_.notify_all();
    work();
    std::unique_lock lock(mutex_);
    done_.wait(lock, [&] { return remaining_ == 0; });
    job_ = {};
  }

private:
  void work() {
    for (;;) {
      int begin = next_.fetch_add(band_, std::memory_order_relaxed);
      if (begin >= rows_)
        return;
      job_(begin, std::min(begin + band_, rows_));
    }
  }
  void run() {
    uint64_t seen = 0;
    for (;;) {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [&] { return stopping_ || generation_ != seen; });
      if (stopping_)
        return;
      seen = generation_;
      lock.unlock();
      work();
      lock.lock();
      if (--remaining_ == 0)
        done_.notify_one();
    }
  }
  std::mutex mutex_, serial_;
  std::condition_variable wake_, done_;
  std::vector<std::thread> threads_;
  std::function<void(int, int)> job_;
  std::atomic<int> next_{0};
  int remaining_ = 0, rows_ = 0, band_ = 32;
  uint64_t generation_ = 0;
  bool stopping_ = false;
};
} // namespace
void parallelRenderRows(int rows, int band, const std::function<void(int, int)> &job) {
  static Workers workers;
  workers.rows(rows, band, job);
}
} // namespace a2
