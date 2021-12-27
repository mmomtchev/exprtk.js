#pragma once

#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <napi.h>

namespace exprtk_js {

const char asyncResourceName[] = "ExprTk.js:async";

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

template <class T> class ExprTkAsyncWorker {
    public:
  typedef std::function<T()> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;

  explicit ExprTkAsyncWorker(
    Napi::Function &callback,
    const MainFunc &doit,
    const RValFunc &rval,
    const std::map<std::string, Napi::Object> &objects,
    std::mutex &lock);

  static void OnExecute(napi_env, void *this_pointer);
  static void CallJS(napi_env env, napi_value js_callback, void *context, void *data);
  void OnFinish();
  void Queue();

    private:
  const MainFunc doit;
  const RValFunc rval;
  T raw;
  const char *err;
  std::map<std::string, Napi::ObjectReference> persistent;
  std::mutex &asyncLock;
  Napi::Env env;
  Napi::Reference<Napi::Function> callbackRef;
  napi_threadsafe_function callbackGate;
  napi_async_work uvWorkHandle;
  Napi::Object asyncResource;
};

template <class T>
ExprTkAsyncWorker<T>::ExprTkAsyncWorker(
  Napi::Function &callback,
  const MainFunc &doit,
  const RValFunc &rval,
  const std::map<std::string, Napi::Object> &objects,
  std::mutex &lock)

  : doit(doit),
    rval(rval),
    err(nullptr),
    asyncLock(lock),
    env(callback.Env()),
    callbackRef(Napi::Persistent(callback)),
    asyncResource(Napi::Object::New(env)) {

  Napi::Value resource_id = Napi::String::New(env, asyncResourceName);

  napi_status status = napi_create_threadsafe_function(
    env, callback, asyncResource, resource_id, 0, 1, nullptr, nullptr, this, CallJS, &callbackGate);
  if ((status) != napi_ok) throw Napi::Error::New(env);

  status = napi_create_async_work(env, asyncResource, resource_id, OnExecute, nullptr, this, &uvWorkHandle);
  if ((status) != napi_ok) throw Napi::Error::New(env);

  for (auto i = objects.begin(); i != objects.end(); i++) persistent[i->first] = Napi::Persistent(i->second);
}

template <class T>
void ExprTkAsyncWorker<T>::CallJS(napi_env env, napi_value js_callback, void *context, void *data) {
  // Here we are back in the main V8 thread, JS is not running
  ExprTkAsyncWorker *self = static_cast<ExprTkAsyncWorker *>(context);
  auto cb = Napi::Function(env, js_callback);
  if (self->err == nullptr) {
    cb.Call({Napi::Env(env).Null(), self->rval(self->raw)});
  } else {
    cb.Call({Napi::Error::New(env, self->err).Value()});
  }
  napi_release_threadsafe_function(self->callbackGate, napi_tsfn_release);
  self->OnFinish();
}

template <class T> void ExprTkAsyncWorker<T>::OnExecute(napi_env, void *this_pointer) {
  // Here we are in the aux thread, JS is running
  ExprTkAsyncWorker *self = static_cast<ExprTkAsyncWorker *>(this_pointer);
  try {
    std::lock_guard<std::mutex> lock(self->asyncLock);
    self->raw = self->doit();
  } catch (const char *err) {
    self->err = err;
  }
  // This will trigger CallJS in the main thread
  napi_call_threadsafe_function(self->callbackGate, nullptr, napi_tsfn_nonblocking);
}

template <class T> void ExprTkAsyncWorker<T>::OnFinish() {
  napi_delete_async_work(env, uvWorkHandle);
  delete this;
}

template <class T> void ExprTkAsyncWorker<T>::Queue() {
  napi_status status = napi_queue_async_work(env, uvWorkHandle);
  if (status != napi_ok) throw Napi::Error::New(env);
}

template <class T> class ExprTkJob {
    public:
  typedef std::function<T()> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;
  MainFunc main;
  RValFunc rval;

  ExprTkJob(std::mutex &lock) : main(), rval(), persistent(), autoIndex(0), asyncLock(lock){};

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
      auto worker = new ExprTkAsyncWorker<T>(callback, main, rval, persistent, asyncLock);
      worker->Queue();
      return info.Env().Undefined();
    }
    try {
      std::lock_guard<std::mutex> lock(asyncLock);
      T obj = main();
      return rval(obj);
    } catch (const char *err) {
      Napi::Error::New(info.Env(), err).ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
  }

    private:
  std::map<std::string, Napi::Object> persistent;
  unsigned autoIndex;
  std::mutex &asyncLock;
};

}; // namespace exprtk_js
