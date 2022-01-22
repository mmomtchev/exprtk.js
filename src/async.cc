#include "async.h"

using namespace exprtk_js;

std::mutex exprtk_js::global_work_mutex;
std::queue<GenericJoblet *> exprtk_js::global_work_queue;
std::condition_variable exprtk_js::global_condition;

std::vector<std::thread> workers;

static bool theEnd = false;

void threadsDestructor() {
  theEnd = true;
  global_condition.notify_all();

  for (auto &worker : workers) worker.join();
}

void workerThread() {
  while (!theEnd) {
    GenericJoblet *j;
    {
      std::unique_lock<std::mutex> lock(global_work_mutex);
      global_condition.wait(lock, [] { return !global_work_queue.empty() || theEnd; });
      if (theEnd) return;
      j = global_work_queue.front();
      global_work_queue.pop();
    }
    j->OnExecute();
  }
}

void exprtk_js::initAsyncWorkers(size_t threads) {
  std::atexit(threadsDestructor);
  for (size_t i = 0; i < threads; i++) workers.push_back(std::thread(workerThread));
}
