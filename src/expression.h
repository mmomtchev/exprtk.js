#pragma once

#include "async.h"
#include "exprtk_instance.h"
#include <map>
#include <mutex>
#include <functional>
#include <napi.h>

namespace exprtk_js {

class Expression : public Napi::ObjectWrap<Expression> {
    public:
  Expression(const Napi::CallbackInfo &);
  ~Expression();

  ASYNCABLE_DECLARE(eval);
  ASYNCABLE_DECLARE(fn);

  static Napi::Function GetClass(Napi::Env);

    private:
  // ExprTk stuff
  std::string expressionText;
  exprtk::symbol_table<double> symbolTable;
  exprtk::expression<double> expression;
  // These are the vectorViews needed for rebasing the vectors when evaluating
  // Read "SECTION 14" of the ExprTk manual for more information on this
  std::map<std::string, exprtk::vector_view<double> *> vectorViews;

  std::mutex asyncLock;

  // Helpers

  // Check a user supplied argument and return a function that imports it into the symbol table
  // Actual importing is deferred to right before the evaluation which might be waiting on an async lock
  template <typename T>
  inline std::function<void()>
  importValue(const Napi::Env &env, ExprTkJob<T> &job, const std::string &name, const Napi::Value &value) const {
    if (value.IsTypedArray()) {
      if (vectorViews.count(name) == 0) {
        throw Napi::TypeError::New(env, name + " is not a declared vector variable");
      }
      auto v = vectorViews.at(name);
      if (value.As<Napi::TypedArray>().TypedArrayType() != napi_float64_array) {
        throw Napi::TypeError::New(env, "vector data must be a Float64Array");
      }
      Napi::TypedArray data = value.As<Napi::TypedArray>();

      if (v->size() != data.ElementLength()) {
        throw Napi::TypeError::New(
          env,
          "vector " + name + " size " + std::to_string(data.ElementLength()) +
            " does not match declared size " + std::to_string(v->size()));
      }

      double *raw = reinterpret_cast<double *>(data.ArrayBuffer().Data());
      job.persist(data);
      return [v, raw]() { v->rebase(raw); };
    } 

    if (value.IsNumber()) {
      auto v = symbolTable.get_variable(name);
      if (v == nullptr) { throw Napi::TypeError::New(env, name + " is not a declared variable"); }
      double raw = value.As<Napi::Number>().DoubleValue();
      return [v, raw]() { v->ref() = raw; };
    }

    throw Napi::TypeError::New(env, name + " is not a number or a TypedArray");
  }
};

}; // namespace exprtk_js
