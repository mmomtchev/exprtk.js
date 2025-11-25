#pragma once

#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#include <napi.h>

#include "semaphore.h"

namespace exprtk_js {

static constexpr char asyncResourceName[] = "ExprTk.js:async";

// This generates method definitions for 2 methods: sync and async version and a hidden common block
#define ASYNCABLE_DEFINE(prefix, method)                                                                               \
  prefix Napi::Value method(const Napi::CallbackInfo &info) {                                                          \
    return method##_do(info, false);                                                                                   \
  }                                                                                                                    \
  prefix Napi::Value method##Async(const Napi::CallbackInfo &info) {                                                   \
    return method##_do(info, true);                                                                                    \
  }                                                                                                                    \
  prefix Napi::Value method##_do(const Napi::CallbackInfo &info, bool async)

// This generates method declarations for 2 methods: sync and async version and a hidden common block
#define ASYNCABLE_DECLARE(method)                                                                                      \
  Napi::Value method(const Napi::CallbackInfo &);                                                                      \
  Napi::Value method##Async(const Napi::CallbackInfo &);                                                               \
  Napi::Value method##_do(const Napi::CallbackInfo &info, bool async)

#define ASYNCABLE_INSTANCE_METHOD(klass, method, props)                                                                \
  klass::InstanceMethod(#method, &klass::method, props),                                                               \
    klass::InstanceMethod(#method "Async", &klass::method##Async, props)

template <class T> class Expression;
template <class T> struct ExpressionInstance;
template <class T> class InstanceGuard;
struct GenericJoblet;

class GenericWorker {
    public:
  virtual void OnExecute(GenericJoblet *j) = 0;
  virtual ~GenericWorker() = default;
};

template <class T> class AsyncWorker;
extern std::mutex global_work_mutex;
extern std::condition_variable global_condition;
extern std::queue<GenericJoblet *> global_work_queue;

/**
 * A Joblet is a single thread splinter of a Job
 */
struct GenericJoblet {
  GenericWorker *worker;
  size_t id;
  inline void enqueue() {
    std::unique_lock<std::mutex> globalLock(global_work_mutex);
    global_work_queue.push(this);
    globalLock.unlock();
    global_condition.notify_one();
  }
  virtual void OnExecute() = 0;
  virtual ~GenericJoblet() = default;
};

template <class T> struct Joblet : public GenericJoblet {
  ExpressionInstance<T> *instance;

  virtual void OnExecute() {
    worker->OnExecute(this);
  }
};

// This a Worker that handles running a job in multiple threads
template <class T> class Worker : public GenericWorker {
    public:
  typedef std::function<T(const ExpressionInstance<T> &, size_t)> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;

  explicit Worker(Expression<T> *e, const MainFunc &doit, const RValFunc &rval, size_t joblets);
  virtual ~Worker() = default;

  virtual void OnExecute(GenericJoblet *j);
  virtual void OnFinish() = 0;
  void Queue();

  inline T Result() {
    return raw;
  }
  inline const char *Error() {
    return err;
  }

    protected:
  Expression<T> *expression;
  const MainFunc doit;
  const RValFunc rval;

    private:
  T raw;
  const char *err;
  std::vector<Joblet<T>> joblets;
  std::atomic_size_t jobletsReady;
};

template <class T>
Worker<T>::Worker(Expression<T> *e, const MainFunc &doit, const RValFunc &rval, size_t nJoblets)

  : expression(e), doit(doit), rval(rval), err(nullptr), joblets(nJoblets), jobletsReady(0) {

  for (size_t i = 0; i < nJoblets; i++) {
    joblets[i].worker = this;
    joblets[i].id = i;
    joblets[i].instance = nullptr;
  }
}

template <class T> void Worker<T>::OnExecute(GenericJoblet *j) {
  auto *joblet = reinterpret_cast<Joblet<T> *>(j);
  // Here we are in the aux thread, JS is running
  try {
    raw = doit(*joblet->instance, joblet->id);
  } catch (const char *err) { this->err = err; }

  auto *w = expression->dequeue();
  if (w != nullptr) {
    w->instance = joblet->instance;
    // TODO this requires one iteration of the worker thread
    // which can be avoided -> we can immediately run the next job
    // but in this case the scheduling won't be fair unless
    // some other mechanism is implemented

    // This is what is not possible with the default Node.js async mechanism:
    // enqueue a new job in the aux thread
    w->enqueue();
  } else {
    expression->releaseIdleInstance(joblet->instance);
  }

  size_t size = joblets.size();
  // This is a high performance lockless barrier
  size_t ready = jobletsReady.fetch_add(1);
  // Only one thread can have ready + 1 = size
  // And this means that all threads executed the previous line
  // From now on `this` can potentially be already deleted
  // (OnFinish() will delete it)
  if (ready + 1 == size) OnFinish();
}

template <class T> void Worker<T>::Queue() {
  for (auto &j : joblets) {
    ExpressionInstance<T> *i = expression->getIdleInstance();
    if (i != nullptr) {
      // There is an idle instance in this Expression
      // -> enqueue on the master queue for immediate execution
      j.instance = i;
      j.enqueue();
      continue;
    }
    // There is no idle instance in this Expression
    // -> enqueue on the Expression local queue
    // OnExecute will dequeue it when an instance is freed
    expression->enqueue(&j);
  }
}

// This a Worker that handles running a job in multiple threads,
// handles persistence, and calls a JS callback
// This worker deletes itself on completion
template <class T> class AsyncWorker : public Worker<T> {
    public:
  using typename Worker<T>::MainFunc;
  using typename Worker<T>::RValFunc;

  explicit AsyncWorker(
    Expression<T> *e,
    Napi::Function &callback,
    const MainFunc &doit,
    const RValFunc &rval,
    size_t joblets,
    const std::map<std::string, Napi::Object> &objects);
  virtual ~AsyncWorker();

  virtual void OnFinish();

    private:
  static void CallJS(napi_env env, napi_value js_callback, void *context, void *data);
  std::map<std::string, Napi::ObjectReference> persistent;
  Napi::Env env;
  Napi::Reference<Napi::Function> callbackRef;
  napi_threadsafe_function callbackGate;
  Napi::Object asyncResource;
};

template <class T>
AsyncWorker<T>::AsyncWorker(
  Expression<T> *e,
  Napi::Function &callback,
  const MainFunc &doit,
  const RValFunc &rval,
  size_t nJoblets,
  const std::map<std::string, Napi::Object> &objects)

  : Worker<T>(e, doit, rval, nJoblets), env(callback.Env()), callbackRef(Napi::Persistent(callback)) {

  Napi::String asyncResourceNameObject = Napi::String::New(env, asyncResourceName);
  napi_status status = napi_create_threadsafe_function(
    env, callback, nullptr, asyncResourceNameObject, 0, 1, nullptr, nullptr, this, CallJS, &callbackGate);
  if ((status) != napi_ok) throw Napi::Error::New(env);

  for (auto const &i : objects) persistent[i.first] = Napi::Persistent(i.second);
}

template <class T> AsyncWorker<T>::~AsyncWorker() {
  napi_release_threadsafe_function(callbackGate, napi_tsfn_release);
}

template <class T> void AsyncWorker<T>::OnFinish() {
  // This will trigger CallJS in the main thread
  napi_call_threadsafe_function(callbackGate, nullptr, napi_tsfn_blocking);
}

template <class T> void AsyncWorker<T>::CallJS(napi_env env, napi_value js_callback, void *context, void *data) {
  // Here we are back in the main V8 thread, JS is not running
  auto *self = static_cast<AsyncWorker<T> *>(context);
  try {
    // If the JS callback throws, MakeCallback will throw a JS Error object as a C++ exception
    // Normally node-addon-api handles these, but not in this case
    auto cb = Napi::Function(env, js_callback);
    if (self->Error() == nullptr) {
      cb.MakeCallback(self->expression->Value(), {Napi::Env(env).Null(), self->rval(self->Result())}, nullptr);
    } else {
      cb.MakeCallback(self->expression->Value(), {Napi::Error::New(env, self->Error()).Value()}, nullptr);
    }
  } catch (const Napi::Error &e) {
    // Alas, there is currently no way to properly terminate the Node process
    // on an unhandled async exception
    fprintf(stderr, "Unhandled exception in async callback: %s\n", e.Message().c_str());
    exit(1);
  }
  delete self;
}

// This a Worker that handles running a job in multiple threads
// and unlocks a semaphore on completion
// This worker does not delete itself on completion
template <class T> class SyncWorker : public Worker<T> {
    public:
  using typename Worker<T>::MainFunc;
  using typename Worker<T>::RValFunc;

  explicit SyncWorker(Expression<T> *e, Semaphore &sem, const MainFunc &doit, const RValFunc &rval, size_t joblets);
  virtual ~SyncWorker() = default;

  virtual void OnFinish();

    private:
  Semaphore &sem;
};

template <class T>
SyncWorker<T>::SyncWorker(Expression<T> *e, Semaphore &sem, const MainFunc &doit, const RValFunc &rval, size_t nJoblets)
  : Worker<T>(e, doit, rval, nJoblets), sem(sem) {
}

template <class T> void SyncWorker<T>::OnFinish() {
  sem.unlock();
}

template <class T> class Job {
    public:
  typedef std::function<T(const ExpressionInstance<T> &, size_t)> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;
  MainFunc main;
  RValFunc rval;
  size_t joblets;

  Job(Expression<T> *e) : main(), rval(), joblets(1), expression(e), persistent(), autoIndex(0) {};

  inline void persist(const std::string &key, const Napi::Object &obj) {
    persistent[key] = obj;
  }

  inline void persist(const Napi::Object &obj) {
    persistent[std::to_string(autoIndex++)] = obj;
  }

  inline void persist(const std::vector<Napi::Object> &objs) {
    for (auto const &i : objs) persist(i);
  }

  Napi::Value run(const Napi::CallbackInfo &info, bool async, int cb_arg) {
    if (!info.This().IsEmpty() && info.This().IsObject()) persist("this", info.This().As<Napi::Object>());
    if (async) {
      // Asynchronous execution by an AsyncWorker that will trigger a JS callback
      if (!info[cb_arg].IsFunction()) {
        Napi::TypeError::New(info.Env(), "The callback must be a function").ThrowAsJavaScriptException();
        return info.Env().Undefined();
      }
      Napi::Function callback = info[cb_arg].As<Napi::Function>();
      auto worker = new AsyncWorker<T>(expression, callback, main, rval, joblets, persistent);
      worker->Queue();
      return info.Env().Undefined();
    }
    if (joblets > 1) {
      // Synchronous multithreaded execution by an SyncWorker that will trigger
      // a C++ callback that will unlock a semaphore blocking the return to JS
      // C++ does not have semaphores until C++20 so a condition variable is used
      Semaphore sem(true); // initialized locked
      auto worker = new SyncWorker<T>(expression, sem, main, rval, joblets);
      worker->Queue(); // will unlock it
      sem.lock();      // wait for the unlock
      if (worker->Error() != nullptr) {
        Napi::Error::New(info.Env(), worker->Error()).ThrowAsJavaScriptException();
        return info.Env().Undefined();
      }
      T obj = worker->Result();
      delete worker;
      return rval(obj);
    }
    // Synchronous monothreaded execution in the main thread
    try {
      InstanceGuard<T> i(expression);
      T obj = main(*i(), 0);
      return rval(obj);
    } catch (const char *err) {
      Napi::Error::New(info.Env(), err).ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
  }

    private:
  Expression<T> *expression;
  std::map<std::string, Napi::Object> persistent;
  unsigned autoIndex;
};

void initAsyncWorkers(size_t threads);

}; // namespace exprtk_js
