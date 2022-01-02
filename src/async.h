#pragma once

#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <napi.h>

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

class GenericWorker {
    public:
  virtual void OnExecute() = 0;
};

extern std::queue<GenericWorker *> global_work_queue;
extern std::mutex global_work_mutex;
extern std::condition_variable global_condition;

template <class T> class ExprTkAsyncWorker : public GenericWorker {
    public:
  typedef std::function<T(const ExpressionInstance<T> &)> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;

  explicit ExprTkAsyncWorker(
    Expression<T> *e,
    Napi::Function &callback,
    const MainFunc &doit,
    const RValFunc &rval,
    const std::map<std::string, Napi::Object> &objects);
  virtual ~ExprTkAsyncWorker();

  virtual void OnExecute();
  static void OnComplete(napi_env, napi_status, void *this_pointer);
  static void CallJS(napi_env env, napi_value js_callback, void *context, void *data);
  void OnFinish();
  void Queue();

    private:
  Expression<T> *expression;
  ExpressionInstance<T> *instance;
  const MainFunc doit;
  const RValFunc rval;
  T raw;
  const char *err;
  std::map<std::string, Napi::ObjectReference> persistent;
  Napi::Env env;
  Napi::Reference<Napi::Function> callbackRef;
  napi_threadsafe_function callbackGate;
  Napi::Object asyncResource;
};

template <class T>
ExprTkAsyncWorker<T>::ExprTkAsyncWorker(
  Expression<T> *e,
  Napi::Function &callback,
  const MainFunc &doit,
  const RValFunc &rval,
  const std::map<std::string, Napi::Object> &objects)

  : expression(e),
    instance(nullptr),
    doit(doit),
    rval(rval),
    err(nullptr),
    env(callback.Env()),
    callbackRef(Napi::Persistent(callback)) {

  Napi::String asyncResourceNameObject = Napi::String::New(env, asyncResourceName);
  napi_status status = napi_create_threadsafe_function(
    env, callback, nullptr, asyncResourceNameObject, 0, 1, nullptr, nullptr, this, CallJS, &callbackGate);
  if ((status) != napi_ok) throw Napi::Error::New(env);

  for (auto const &i : objects) persistent[i.first] = Napi::Persistent(i.second);
}

template <class T> ExprTkAsyncWorker<T>::~ExprTkAsyncWorker() {
  napi_release_threadsafe_function(callbackGate, napi_tsfn_release);
}

template <class T> void ExprTkAsyncWorker<T>::CallJS(napi_env env, napi_value js_callback, void *context, void *data) {
  // Here we are back in the main V8 thread, JS is not running
  auto *self = static_cast<ExprTkAsyncWorker<T> *>(context);
  try {
    // If the JS callback throws, MakeCallback will throw a JS Error object as a C++ exception
    // Normally node-addon-api handles these, but not in this case
    auto cb = Napi::Function(env, js_callback);
    if (self->err == nullptr) {
      cb.MakeCallback(self->expression->Value(), {Napi::Env(env).Null(), self->rval(self->raw)}, nullptr);
    } else {
      cb.MakeCallback(self->expression->Value(), {Napi::Error::New(env, self->err).Value()}, nullptr);
    }
  } catch (const Napi::Error &e) { 
    // Alas, there is currently no way to properly terminate the Node process
    // on an unhandled async exception
    fprintf(stderr, "Unhandled exception in async callback: %s\n", e.Message().c_str());
    exit(1);
  }
  delete self;
}

template <class T> void ExprTkAsyncWorker<T>::OnExecute() {
  // Here we are in the aux thread, JS is running
  try {
    raw = doit(*instance);

    if (!expression->work_queue.empty()) {
      // TODO this requires one iterator of the worker thread
      // which can be avoided -> we can immediately run the next job

      // This is what is not possible with the default Node.js async mechanism:
      // enqueue a new job in the aux thread
      auto *w = expression->work_queue.front();
      expression->work_queue.pop();
      std::unique_lock<std::mutex> globalLock(global_work_mutex);
      w->instance = this->instance;
      global_work_queue.push(w);
      globalLock.unlock();
      global_condition.notify_one();
    } else {
      expression->releaseIdleInstance(instance);
    }

  } catch (const char *err) { this->err = err; }
  // This will trigger CallJS in the main thread
  napi_call_threadsafe_function(callbackGate, nullptr, napi_tsfn_nonblocking);
}

template <class T> void ExprTkAsyncWorker<T>::Queue() {
  ExpressionInstance<T> *i = expression->getIdleInstance();
  if (i != nullptr) {
    // There is an idle instance in this Expression
    // -> enqueue on the master queue for immediate execution
    this->instance = i;
    std::unique_lock<std::mutex> globalLock(global_work_mutex);
    global_work_queue.push(this);
    globalLock.unlock();
    global_condition.notify_one();
    return;
  }
  // There is no idle instance in this Expression
  // -> enqueue on the local queue
  // OnExecute will dequeue it
  expression->work_queue.push(this);
}

template <class T> class ExprTkJob {
    public:
  typedef std::function<T(const ExpressionInstance<T> &)> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;
  MainFunc main;
  RValFunc rval;

  ExprTkJob(Expression<T> *e) : main(), rval(), expression(e), persistent(), autoIndex(0){};

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
      if (!info[cb_arg].IsFunction()) {
        Napi::TypeError::New(info.Env(), "The callback must be a function").ThrowAsJavaScriptException();
        return info.Env().Undefined();
      }
      Napi::Function callback = info[cb_arg].As<Napi::Function>();
      auto worker = new ExprTkAsyncWorker<T>(expression, callback, main, rval, persistent);
      worker->Queue();
      return info.Env().Undefined();
    }
    try {
      ExpressionInstance<T> *i;
      do { i = expression->getIdleInstance(); } while (i == nullptr);
      T obj = main(*i);
      expression->releaseIdleInstance(i);
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
