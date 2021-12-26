#pragma once

#include <exprtk.hpp>
#include <map>
#include <mutex>
#include <functional>
#include <napi.h>

#include <exprtkjs.h>
#include "async.h"

namespace exprtk_js {

template <typename T> struct NapiArrayType {};

template <> struct NapiArrayType<int8_t> {
  static const napi_typedarray_type type = napi_int8_array;
  static constexpr const char *name = "Int8";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Int8Array::New(env, elementLength);
  }
  static inline int8_t CastFrom(const Napi::Value &value) {
    return static_cast<int8_t>(value.As<Napi::Number>().Int32Value());
  }
};

template <> struct NapiArrayType<uint8_t> {
  static const napi_typedarray_type type = napi_uint8_array;
  static constexpr const char *name = "Uint8";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Uint8Array::New(env, elementLength);
  }
  static inline uint8_t CastFrom(const Napi::Value &value) {
    return static_cast<uint8_t>(value.As<Napi::Number>().Uint32Value());
  }
};

template <> struct NapiArrayType<int16_t> {
  static const napi_typedarray_type type = napi_int16_array;
  static constexpr const char *name = "Int16";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Int16Array::New(env, elementLength);
  }
  static inline int16_t CastFrom(const Napi::Value &value) {
    return static_cast<int16_t>(value.As<Napi::Number>().Int32Value());
  }
};

template <> struct NapiArrayType<uint16_t> {
  static const napi_typedarray_type type = napi_uint16_array;
  static constexpr const char *name = "Uint16";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Uint16Array::New(env, elementLength);
  }
  static inline uint16_t CastFrom(const Napi::Value &value) {
    return static_cast<uint16_t>(value.As<Napi::Number>().Uint32Value());
  }
};

template <> struct NapiArrayType<int32_t> {
  static const napi_typedarray_type type = napi_int32_array;
  static constexpr const char *name = "Int32";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Int32Array::New(env, elementLength);
  }
  static inline int32_t CastFrom(const Napi::Value &value) {
    return value.As<Napi::Number>().Int32Value();
  }
};

template <> struct NapiArrayType<uint32_t> {
  static const napi_typedarray_type type = napi_uint32_array;
  static constexpr const char *name = "Uint32";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Uint32Array::New(env, elementLength);
  }
  static inline uint32_t CastFrom(const Napi::Value &value) {
    return value.As<Napi::Number>().Uint32Value();
  }
};
template <> struct NapiArrayType<double> {
  static const napi_typedarray_type type = napi_float64_array;
  static constexpr const char *name = "Float64";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Float64Array::New(env, elementLength);
  }
  static inline double CastFrom(const Napi::Value &value) {
    return value.As<Napi::Number>().DoubleValue();
  }
};

template <> struct NapiArrayType<float> {
  static const napi_typedarray_type type = napi_float32_array;
  static constexpr const char *name = "Float32";
  static inline Napi::TypedArray New(napi_env env, size_t elementLength) {
    return Napi::Float32Array::New(env, elementLength);
  }
  static inline float CastFrom(const Napi::Value &value) {
    return value.As<Napi::Number>().FloatValue();
  }
};

template <typename T> class Expression : public Napi::ObjectWrap<Expression<T>> {
    public:
  Expression(const Napi::CallbackInfo &);
  ~Expression();

  ASYNCABLE_DECLARE(eval);
  ASYNCABLE_DECLARE(map);
  ASYNCABLE_DECLARE(reduce);
  ASYNCABLE_DECLARE(cwise);

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
  Napi::Value GetType(const Napi::CallbackInfo &info);
  Napi::Value GetScalars(const Napi::CallbackInfo &info);
  Napi::Value GetVectors(const Napi::CallbackInfo &info);
  Napi::Value GetCAPI(const Napi::CallbackInfo &info);

  static Napi::Function GetClass(Napi::Env);

    private:
  // ExprTk stuff
  std::string expressionText;
  exprtk::symbol_table<T> symbolTable;
  exprtk::expression<T> expression;
  // These are the vectorViews needed for rebasing the vectors when evaluating
  // Read "SECTION 14" of the ExprTk manual for more information on this
  std::map<std::string, exprtk::vector_view<T> *> vectorViews;

  // this one is prone to static initialization fiasco
  // there is a single shared instance per data type
  inline exprtk::parser<T> &parser() {
    static exprtk::parser<T> _parser;

    return _parser;
  }

  // get_variable_list / get_vector_list do not conserve the initial order
  std::vector<std::string> variableNames;

  // Expression is not reentrant because of the symbol table
  std::mutex asyncLock;

  // This is a persistent reference to the CAPI object of this Expression
  std::shared_ptr<Napi::Reference<Napi::ArrayBuffer>> capiDescriptor;

  // Helpers

  // Check a user supplied argument and return a function that imports it into the symbol table
  // Actual importing is deferred to right before the evaluation which might be waiting on an async lock
  void importValue(
    const Napi::Env &env,
    ExprTkJob<T> &job,
    const std::string &name,
    const Napi::Value &value,
    std::vector<std::function<void()>> &importers) const {
    if (value.IsTypedArray()) {
      if (vectorViews.count(name) == 0) {
        throw Napi::TypeError::New(env, name + " is not a declared vector variable");
      }
      auto v = vectorViews.at(name);
      if (value.As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {
        throw Napi::TypeError::New(env, "vector data must be a " + std::string(NapiArrayType<T>::name) + "Array");
      }
      Napi::TypedArray data = value.As<Napi::TypedArray>();

      if (v->size() != data.ElementLength()) {
        throw Napi::TypeError::New(
          env,
          "vector " + name + " size " + std::to_string(data.ElementLength()) + " does not match declared size " +
            std::to_string(v->size()));
      }

      T *raw = reinterpret_cast<T *>(data.ArrayBuffer().Data());
      job.persist(data);
      importers.push_back([v, raw]() { v->rebase(raw); });
      return;
    }

    if (value.IsNumber()) {
      auto v = symbolTable.get_variable(name);
      if (v == nullptr) { throw Napi::TypeError::New(env, name + " is not a declared scalar variable"); }
      T raw = NapiArrayType<T>::CastFrom(value);
      importers.push_back([v, raw]() { v->ref() = raw; });
      return;
    }

    throw Napi::TypeError::New(env, name + " is not a number or a TypedArray");
  }

  inline void importFromObject(
    const Napi::Env &env,
    ExprTkJob<T> &job,
    const Napi::Value &object,
    std::vector<std::function<void()>> &importers) const {

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
    ExprTkJob<T> &job,
    const Napi::CallbackInfo &info,
    size_t firstArg,
    size_t lastArg,
    std::vector<std::function<void()>> &importers,
    const std::set<std::string> &skip = {}) const {

    size_t i = firstArg;
    for (auto const &v : variableNames) {
      if (skip.count(v) > 0) continue;
      importValue(env, job, v, info[i], importers);
      i++;
      if (i == lastArg) return;
    }
  }
};

}; // namespace exprtk_js
