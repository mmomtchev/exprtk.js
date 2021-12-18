#pragma once

#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <napi.h>

namespace exprtk_js {

// Shamelessly stolen from the awesome node-gdal-async

// This generates method definitions for 2 methods: sync and async version and a hidden common block
#define ASYNCABLE_DEFINE(method)                                                                                       \
  Napi::Value method(const Napi::CallbackInfo &info) {                                                                 \
    return method##_do(info, false);                                                                                   \
  }                                                                                                                    \
  Napi::Value method##Async(const Napi::CallbackInfo &info) {                                                          \
    return method##_do(info, true);                                                                                    \
  }                                                                                                                    \
  Napi::Value method##_do(const Napi::CallbackInfo &info, bool async)

// This generates method declarations for 2 methods: sync and async version and a hidden common block
#define ASYNCABLE_DECLARE(method)                                                                                      \
  Napi::Value method(const Napi::CallbackInfo &);                                                                      \
  Napi::Value method##Async(const Napi::CallbackInfo &);                                                               \
  Napi::Value method##_do(const Napi::CallbackInfo &info, bool async)

#define ASYNCABLE_INSTANCE_METHOD(klass, method, props)                                                                \
  klass::InstanceMethod(#method, &klass::method, props),                                                               \
    klass::InstanceMethod(#method "Async", &klass::method##Async, props)

template <class T> class ExprTkAsyncWorker : public Napi::AsyncWorker {
    public:
  typedef std::function<T()> MainFunc;
  typedef std::function<Napi::Value(const T)> RValFunc;

  explicit ExprTkAsyncWorker(
    Napi::Function &callback,
    const MainFunc &doit,
    const RValFunc &rval,
    const std::map<std::string, Napi::Object> &objects,
    std::mutex &lock);

  void Execute();
  void OnOK();

    private:
  const MainFunc doit;
  const RValFunc rval;
  T raw;
  std::map<std::string, Napi::ObjectReference> persistent;
  std::mutex &asyncLock;
};

template <class T>
ExprTkAsyncWorker<T>::ExprTkAsyncWorker(
  Napi::Function &callback,
  const MainFunc &doit,
  const RValFunc &rval,
  const std::map<std::string, Napi::Object> &objects,
  std::mutex &lock)
  : AsyncWorker(callback, "ExprTk.js:ExprTkAsyncWorker"), doit(doit), rval(rval), asyncLock(lock) {

  for (auto i = objects.begin(); i != objects.end(); i++) persistent[i->first] = Napi::Persistent(i->second);
}

template <class T> void ExprTkAsyncWorker<T>::Execute() {
  try {
    std::lock_guard<std::mutex> lock(asyncLock);
    raw = doit();
  } catch (const char *err) { SetError(err); }
}

template <class T> void ExprTkAsyncWorker<T>::OnOK() {
  Callback().Call({Env().Null(), rval(raw)});
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
