#pragma once

#include <exprtk.hpp>
#include <map>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include <queue>
#include <napi.h>

#include <exprtkjs.h>

#include "async.h"
#include "types.h"
#include "ndarray.h"

namespace exprtk_js {

template <class T> struct ExpressionInstance {
  exprtk::symbol_table<T> symbolTable;
  bool isInit;
  // These are the vectorViews needed for rebasing the vectors when evaluating
  // Read "SECTION 14" of the ExprTk manual for more information on this
  std::map<std::string, std::unique_ptr<exprtk::vector_view<T>>> vectorViews;
  // This must be the last element because it must be destroyed first
  exprtk::expression<T> expression;
};

template <typename T> class Expression : public Napi::ObjectWrap<Expression<T>> {
    public:
  Expression(const Napi::CallbackInfo &);
  virtual ~Expression();
  void compileInstance(ExpressionInstance<T> *instance);

  ASYNCABLE_DECLARE(eval);
  ASYNCABLE_DECLARE(map);
  ASYNCABLE_DECLARE(reduce);
  ASYNCABLE_DECLARE(cwise);

  Napi::Value ToString(const Napi::CallbackInfo &info);

  exprtk_result capi_eval(const void *scalars, void **vectors, void *result);
  exprtk_result capi_map(
    const char *iterator_name,
    const size_t iterator_len,
    const void *iterator_vector,
    const void *scalars,
    void **vectors,
    void *result);
  exprtk_result capi_reduce(
    const char *iterator_name,
    const size_t iterator_len,
    const void *_iterator_vector,
    const char *accumulator,
    const void *_scalars,
    void **_vectors,
    void *_result);
  exprtk_result capi_cwise(const size_t n_args, const exprtk_capi_cwise_arg *args, exprtk_capi_cwise_arg *result);

  Napi::Value GetExpression(const Napi::CallbackInfo &info);
  Napi::Value GetScalars(const Napi::CallbackInfo &info);
  Napi::Value GetVectors(const Napi::CallbackInfo &info);
  Napi::Value GetCAPI(const Napi::CallbackInfo &info);
  Napi::Value GetMaxParallel(const Napi::CallbackInfo &info);
  void SetMaxParallel(const Napi::CallbackInfo &info, const Napi::Value &value);
  Napi::Value GetMaxActive(const Napi::CallbackInfo &info);

  static Napi::Function GetClass(Napi::Env);

    private:
  // This is the evaluations that are waiting for an evaluation instance
  std::queue<Joblet<T> *> work_queue;
  // This is where synchronous evaluation sleep while waiting for an instance
  std::condition_variable work_condition;

  std::string expressionText;

  size_t maxParallel;
  size_t maxActive;
  size_t currentActive;

  std::mutex asyncLock;

  // ExprTk stuff in multiple instances to support reentrancy
  std::vector<ExpressionInstance<T>> instances;
  std::list<ExpressionInstance<T> *> instancesIdle;

  // this one is prone to static initialization fiasco
  // there is a single shared instance per data type
  inline exprtk::parser<T> &parser() {
    static exprtk::parser<T> _parser;

    return _parser;
  }
  std::mutex parserMutex;

  // get_variable_list / get_vector_list do not conserve the initial order
  std::vector<std::string> variableNames;

  // This is a persistent reference to the CAPI object of this Expression
  std::shared_ptr<Napi::Reference<Napi::ArrayBuffer>> capiDescriptor;

  // Helpers

  // Check a user supplied argument and return a function that imports it into the symbol table
  // Actual importing is deferred to right before the evaluation which might be waiting on an async lock
  void importValue(
    const Napi::Env &env,
    Job<T> &job,
    const std::string &name,
    const Napi::Value &value,
    std::vector<std::function<void(const ExpressionInstance<T> &)>> &importers) const {
    if (value.IsTypedArray()) {
      if (instances[0].vectorViews.count(name) == 0) {
        throw Napi::TypeError::New(env, name + " is not a declared vector variable");
      }
      if (value.As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {
        throw Napi::TypeError::New(env, "vector data must be a " + std::string(NapiArrayType<T>::name) + "Array");
      }
      Napi::TypedArray data = value.As<Napi::TypedArray>();

      if (instances[0].vectorViews.at(name)->size() != data.ElementLength()) {
        throw Napi::TypeError::New(
          env,
          "vector " + name + " size " + std::to_string(data.ElementLength()) + " does not match declared size " +
            std::to_string(instances[0].vectorViews.at(name)->size()));
      }

      T *raw = reinterpret_cast<T *>(data.ArrayBuffer().Data());
      job.persist(data);
      importers.push_back([raw, name](const ExpressionInstance<T> &i) { i.vectorViews.at(name)->rebase(raw); });
      return;
    }

    if (value.IsNumber()) {
      auto v = instances[0].symbolTable.get_variable(name);
      if (v == nullptr) { throw Napi::TypeError::New(env, name + " is not a declared scalar variable"); }
      T raw = NapiArrayType<T>::CastFrom(value);
      importers.push_back([raw, name](const ExpressionInstance<T> &i) {
        auto v = i.symbolTable.get_variable(name);
        v->ref() = raw;
      });
      return;
    }

    throw Napi::TypeError::New(env, name + " is not a number or a TypedArray");
  }

  inline void importFromObject(
    const Napi::Env &env,
    Job<T> &job,
    const Napi::Value &object,
    std::vector<std::function<void(const ExpressionInstance<T> &)>> &importers) const {

    Napi::Object args = object.ToObject();
    Napi::Array argNames = args.GetPropertyNames();

    for (std::size_t i = 0; i < argNames.Length(); i++) {
      const std::string name = argNames.Get(i).As<Napi::String>().Utf8Value();
      Napi::Value value = args.Get(name);
      importValue(env, job, name, value, importers);
    }
  }

  inline void importFromArgumentsArray(
    const Napi::Env &env,
    Job<T> &job,
    const Napi::CallbackInfo &info,
    size_t firstArg,
    size_t lastArg,
    std::vector<std::function<void(const ExpressionInstance<T> &)>> &importers,
    const std::set<std::string> &skip = {}) const {

    size_t i = firstArg;
    for (auto const &v : variableNames) {
      if (skip.count(v) > 0) continue;
      importValue(env, job, v, info[i], importers);
      i++;
      if (i == lastArg) return;
    }
  }

    public:
  inline void enqueue(Joblet<T> *w) {
    std::lock_guard<std::mutex> lock(asyncLock);
    work_queue.push(w);
  }

  inline Joblet<T> *dequeue() {
    std::lock_guard<std::mutex> lock(asyncLock);
    if (work_queue.empty()) return nullptr;
    auto *w = work_queue.front();
    work_queue.pop();
    return w;
  }

  inline ExpressionInstance<T> *getIdleInstance() {
    std::lock_guard<std::mutex> lock(asyncLock);
    if (instancesIdle.empty() || currentActive >= maxParallel) return nullptr;
    auto *r = instancesIdle.front();
    instancesIdle.pop_front();
    currentActive++;
    if (!r->isInit) compileInstance(r);
    return r;
  }

  inline void releaseIdleInstance(ExpressionInstance<T> *i) {
    std::unique_lock<std::mutex> lock(asyncLock);
    instancesIdle.push_front(i);
    currentActive--;
    lock.unlock();
    work_condition.notify_one();
  }

  inline ExpressionInstance<T> *waitIdleInstance() {
    std::unique_lock<std::mutex> lock(asyncLock);
    work_condition.wait(lock, [this] { return !instancesIdle.empty() && currentActive < maxParallel; });
    auto *r = instancesIdle.front();
    instancesIdle.pop_front();
    currentActive++;
    if (!r->isInit) compileInstance(r);
    return r;
  }
};

// A RAII guard for acquiring an ExpressionInstance
// and releasing it when going out of scope
// Used by the synchronous methods
template <typename T> class InstanceGuard {
    public:
  inline InstanceGuard(Expression<T> *e) : expression(e) {
    instance = expression->waitIdleInstance();
  }

  inline ~InstanceGuard() {
    expression->releaseIdleInstance(instance);
  }

  inline ExpressionInstance<T> *operator()() const {
    return instance;
  }

    private:
  Expression<T> *expression;
  ExpressionInstance<T> *instance;
};

}; // namespace exprtk_js
