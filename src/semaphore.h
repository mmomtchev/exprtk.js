#include <condition_variable>
#include <mutex>

// This is a simple binary semaphore that does not exist in C++14
// Reminder: a semaphore can be unlocked by anyone
// while a mutex can be unlocked only by its owner
class Semaphore {
    public:
  Semaphore(bool initial) : mtx(), cond(), busy(initial) {
  }

  Semaphore(const Semaphore &Semaphore) = delete;
  Semaphore(Semaphore &&Semaphore) = delete;
  Semaphore &operator=(const Semaphore &Semaphore) = delete;
  Semaphore &operator=(Semaphore &&Semaphore) = delete;

  void inline lock() {
    std::unique_lock<std::mutex> guard(mtx);
    cond.wait(guard, [this]() { return !busy; });
    busy = true;
  }

  void inline unlock() {
    std::unique_lock<std::mutex> guard(mtx);
    busy = false;
    guard.unlock();
    cond.notify_all();
  }

    private:
  std::mutex mtx;
  std::condition_variable cond;
  bool busy;
};