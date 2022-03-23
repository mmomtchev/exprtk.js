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
    // Normally for best performance the mutex should be released before calling notify_all
    // However this semaphore can guard its own deletion
    // In this case, the condition variable will disappear as soon as the mutex is released
    // https://github.com/mmomtchev/exprtk.js/issues/31
    std::unique_lock<std::mutex> guard(mtx);
    busy = false;
    cond.notify_all();
  }

    private:
  std::mutex mtx;
  std::condition_variable cond;
  bool busy;
};