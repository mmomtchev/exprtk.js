#include "expression.h"

#include <memory>

using namespace exprtk_js;

/**
 * @class Expression
 * @param {string} expression function
 * @param {string[]} [variables] An array containing all the scalar variables' names, will be determined automatically if omitted, however order won't be guaranteed, scalars are passed by value
 * @param {Record<string, number>} [vectors] An object containing all the vector variables' names and their sizes, vector size must be known at compilation (object construction), vectors are passed by reference and can be modified by the ExprTk expression
 * @returns {Expression}
 * @memberof Expression
 * 
 * The `Expression` represents an expression compiled to an AST from a string. Expressions come in different flavors depending on the internal type used.
 * 
 * ---------------------------------------
 * | JS      | C/C++                     |
 * | ------- | ------------------------- |
 * | Float64 | double                    |
 * | Float32 | float                     |
 * | Uint32  | uint32_t (unsigned long)  |
 * | Int32   | int32_t (long)            |
 * | Uint16  | uint16_t (unsigned short) |
 * | Int16   | int16_t (short)           |
 * | Uint8   | uint8_t (unsigned char)   |
 * | Int8    | int8_t (char)             |
 *
 * @example
 * // This determines the internally used type
 * const expr = require("exprtk.js").Float64;
 * 
 * // arithmetic mean of 2 variables
 * const mean = new Expression('(a+b)/2', ['a', 'b']);
 * 
 * // naive stddev of an array of 1024 elements
 * const stddev = new Expression(
 *  'var sum := 0; var sumsq := 0; ' + 
 *  'for (var i := 0; i < x[]; i += 1) { sum += x[i]; sumsq += x[i] * x[i] }; ' +
 *  '(sumsq - (sum*sum) / x[]) / (x[] - 1);',
 *  [], {x: 1024})
 */
template <typename T>
Expression<T>::Expression(const Napi::CallbackInfo &info)
  : Napi::ObjectWrap<Expression<T>>::ObjectWrap(info),
    instances(std::thread::hardware_concurrency() + 1),
    capiDescriptor(nullptr) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "expresion must be a string").ThrowAsJavaScriptException();
    return;
  }
  expressionText = info[0].As<Napi::String>().Utf8Value();

  if (info.Length() > 1) {
    if (!info[1].IsArray()) {
      Napi::TypeError::New(env, "arguments must be an array").ThrowAsJavaScriptException();
      return;
    }
    Napi::Array args = info[1].As<Napi::Array>();
    for (std::size_t i = 0; i < args.Length(); i++) {
      const std::string name = args.Get(i).As<Napi::String>().Utf8Value();
      if (!instances[0].symbolTable.create_variable(name)) {
        Napi::TypeError::New(env, name + " is not a valid variable name").ThrowAsJavaScriptException();
        return;
      }
      variableNames.push_back(name);
    }
  } else {
    std::vector<std::string> args;
    exprtk::collect_variables(expressionText, args);
    for (const auto &name : args) {
      if (!instances[0].symbolTable.create_variable(name)) {
        Napi::TypeError::New(env, name + " is not a valid variable name").ThrowAsJavaScriptException();
        return;
      }
      variableNames.push_back(name);
    }
  }

  if (info.Length() > 2) {
    if (!info[2].IsObject()) {
      Napi::TypeError::New(env, "vectors must be an object").ThrowAsJavaScriptException();
      return;
    }
    Napi::Object args = info[2].As<Napi::Object>();
    Napi::Array argNames = args.GetPropertyNames();
    for (std::size_t i = 0; i < argNames.Length(); i++) {
      const std::string name = argNames.Get(i).As<Napi::String>().Utf8Value();
      Napi::Value value = args.Get(name);

      if (!value.IsNumber()) {
        Napi::TypeError::New(env, "vector size must be a number").ThrowAsJavaScriptException();
        return;
      }

      // We are slightly bending the rules here - a vector view is not supposed to have a nullptr
      // or ExprTk will display some puzzling behaviour (it will allocate it and forget to free it)
      // However it will happily swallow an invalid pointer
      size_t size = value.ToNumber().Int64Value();
      T *dummy = (T *)&size;
      auto *v = new exprtk::vector_view<T>(dummy, size);
      instances[0].vectorViews[name] = v;

      if (!instances[0].symbolTable.add_vector(name, *v)) {
        Napi::TypeError::New(env, name + " is not a valid vector name").ThrowAsJavaScriptException();
        return;
      }

      variableNames.push_back(name);
    }
  }

  instances[0].expression.register_symbol_table(instances[0].symbolTable);

  std::lock_guard<std::mutex> lock(parserMutex);
  if (!parser().compile(expressionText, instances[0].expression)) {
    std::string errorText = "failed compiling expression " + expressionText + "\n";
    for (std::size_t i = 0; i < parser().error_count(); i++) {
      exprtk::parser_error::type error = parser().get_error(i);
      errorText += exprtk::parser_error::to_str(error.mode) + " at " + std::to_string(error.token.position) + " : " +
        error.diagnostic + "\n";
    }
    Napi::Error::New(env, errorText).ThrowAsJavaScriptException();
  }

  instances[0].isInit = true;
  instances[0].isBusy = false;
  for (size_t i = 1; i < std::thread::hardware_concurrency() + 1; i++) {
    instances[1].isInit = false;
    instances[1].isBusy = false;
  }

  maxParallel = std::thread::hardware_concurrency();
}

template <typename T> Expression<T>::~Expression() {
  for (auto &i : instances) {
    if (i.isBusy)
      fprintf(
        stderr,
        "GC waiting on a background evaluation of an Expression object, event loop blocked. "
        "If you are using only the JS interface, this is a bug in ExprTk.js. "
        "If you are using the C/C++ API, you must always protect Expression objects from the GC "
        "by obtaining a persistent object reference. \n");
    if (i.isInit) {
      for (auto const &v : i.vectorViews) {
        // exprtk will sometimes try to free this pointer
        // on object destruction even if it never allocated it
        v.second->rebase((T *)nullptr);
        delete v.second;
      }
      i.vectorViews.clear();
    }
  }
}

template <typename T> void Expression<T>::compileInstance(size_t instance) {
  if (instances[instance].isInit) return;
  for (auto const &name : variableNames) {
    if (instances[0].symbolTable.get_variable(name))
      instances[instance].symbolTable.create_variable(name);
    else {
      auto vector = instances[0].symbolTable.get_vector(name);
      auto size = vector->size();
      T *dummy = (T *)&size;
      auto *v = new exprtk::vector_view<T>(dummy, size);
      instances[instance].vectorViews[name] = v;
      instances[instance].symbolTable.add_vector(name, *v);
    }
  }
  instances[instance].isInit = true;
  instances[instance].isBusy = false;
  instances[instance].expression.register_symbol_table(instances[instance].symbolTable);
  std::lock_guard<std::mutex> lock(parserMutex);
  parser().compile(expressionText, instances[instance].expression);
}

/**
 * Evaluate the expression.
 * 
 * All arrays must match the internal data type.
 *
 * @param {...(number|TypedArray<T>)[]|Record<string, number|TypedArray<T>>} arguments of the function
 * @returns {number}
 * @memberof Expression
 *
 * @example
 * // These two are equivalent
 * const r1 = expr.eval({a: 2, b: 5});  // less error-prone
 * const r2 = expr.eval(2, 5);          // slightly faster
 *
 * // These two are equivalent
 * expr.evalAsync({a: 2, b: 5}, (e,r) => console.log(e, r));
 * expr.evalAsync(2, 5, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::eval) {
  Napi::Env env = info.Env();

  ExprTkJob<T> job(this);

  std::vector<std::function<void(const ExpressionInstance<T> &)>> importers;

  if (info.Length() > 0 && info[0].IsObject() && !info[0].IsTypedArray()) {
    importFromObject(env, job, info[0], importers);
  }

  if (info.Length() > 0 && (info[0].IsNumber() || info[0].IsTypedArray())) {
    size_t last = info.Length();
    if (async && last > 0 && info[last - 1].IsFunction()) last--;
    importFromArgumentsArray(env, job, info, 0, last, importers);
  }

  if (instances[0].symbolTable.variable_count() + instances[0].symbolTable.vector_count() != importers.size()) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  job.main = [importers](const ExpressionInstance<T> &i) {
    for (auto const &f : importers) f(i);
    T r = i.expression.value();
    if (i.expression.results().count()) { throw "explicit return values are not supported"; }
    return r;
  };
  job.rval = [env](T r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T> exprtk_result Expression<T>::capi_eval(const void *_scalars, void **_vectors, void *_result) {
  std::lock_guard<std::mutex> lock(asyncLock);

  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *result = reinterpret_cast<T *>(_result);

  size_t nvars = instances[0].symbolTable.variable_count();
  size_t nvectors = instances[0].symbolTable.vector_count();
  for (size_t i = 0; i < nvars; i++) instances[0].symbolTable.get_variable(variableNames[i])->ref() = scalars[i];
  for (size_t i = 0; i < nvectors; i++) instances[0].vectorViews[variableNames[i + nvars]]->rebase(vectors[i]);
  *result = instances[0].expression.value();
  return exprtk_ok;
}

/**
 * Evaluate the expression for every element of a TypedArray.
 * 
 * Evaluation and traversal happens entirely in C++ so this will be much
 * faster than calling `array.map(expr.eval)`.
 * 
 * All arrays must match the internal data type.
 *
 * @param {TypedArray<T>} array for the expression to be iterated over
 * @param {string} iterator variable name
 * @param {...(number|TypedArray<T>)[]|Record<string, number|TypedArray<T>>} arguments of the function, iterator removed
 * @returns {TypedArray<T>}
 * @memberof Expression
 *
 * @example
 * // Clamp values in an array to [0..1000]
 * const expr = new Expression('clamp(f, x, c)', ['f', 'x', 'c']);
 * 
 * // r will be a TypedArray of the same type
 * // These two are equivalent
 * const r1 = expr.map(array, 'x', 0, 1000);
 * const r2 = expr.map(array, 'x', {f: 0, c: 0});
 *
 * expr.mapAsync(array, 'x', 0, 1000, (e,r) => console.log(e, r));
 * expr.mapAsync(array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::map) {
  Napi::Env env = info.Env();

  ExprTkJob<T> job(this);

  std::vector<std::function<void(const ExpressionInstance<T> &)>> importers;

  if (
    info.Length() < 1 || !info[0].IsTypedArray() ||
    info[0].As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {

    Napi::TypeError::New(env, "first argument must be a " + std::string(NapiArrayType<T>::name) + "Array")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::TypedArray array = info[0].As<Napi::TypedArray>();
  T *input = reinterpret_cast<T *>(array.ArrayBuffer().Data());
  size_t len = array.ElementLength();

  if (info.Length() < 2 || !info[1].IsString()) {
    Napi::TypeError::New(env, "second argument must be the iterator variable name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const std::string iteratorName = info[1].As<Napi::String>().Utf8Value();
  auto iterator = instances[0].symbolTable.get_variable(iteratorName);
  if (iterator == nullptr) {
    Napi::TypeError::New(env, iteratorName + " is not a declared scalar variable").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > 2 && info[2].IsObject() && !info[2].IsTypedArray()) {
    importFromObject(env, job, info[2], importers);
  }

  if (info.Length() > 2 && (info[2].IsNumber() || info[2].IsTypedArray())) {
    size_t last = info.Length();
    if (async && last > 2 && info[last - 1].IsFunction()) last--;
    importFromArgumentsArray(env, job, info, 2, last, importers, {iteratorName});
  }

  if (instances[0].symbolTable.variable_count() + instances[0].symbolTable.vector_count() != importers.size() + 1) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::TypedArray result = NapiArrayType<T>::New(env, len);
  T *output = reinterpret_cast<T *>(result.ArrayBuffer().Data());

  // this should have been an unique_ptr
  // but std::function is not compatible with move semantics
  auto persistent = std::make_shared<Napi::Reference<Napi::TypedArray>>(Napi::Persistent(result));

  job.main = [importers, iteratorName, input, output, len](const ExpressionInstance<T> &i) {
    for (auto const &f : importers) f(i);
    auto iterator = i.symbolTable.get_variable(iteratorName);

    T *it_ptr = &iterator->ref();
    T *in_ptr = input;
    T *out_ptr = output;
    auto const in_end = in_ptr + len;
    auto &expression = i.expression;
    for (; in_ptr < in_end; in_ptr++, out_ptr++) {
      *it_ptr = *in_ptr;
      *out_ptr = expression.value();
    }
    return 0;
  };
  job.rval = [persistent](T r) { return persistent->Value(); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T>
exprtk_result Expression<T>::capi_map(
  const char *iterator_name,
  const size_t iterator_len,
  const void *_iterator_vector,
  const void *_scalars,
  void **_vectors,
  void *_result) {
  std::lock_guard<std::mutex> lock(asyncLock);
  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *it_ptr = nullptr;

  size_t nvars = instances[0].symbolTable.variable_count();
  size_t nvectors = instances[0].symbolTable.vector_count();

  size_t scalars_idx = 0;
  for (size_t i = 0; i < nvars; i++) {
    if (variableNames[i] == iterator_name) {
      it_ptr = &instances[0].symbolTable.get_variable(variableNames[i])->ref();
    } else {
      instances[0].symbolTable.get_variable(variableNames[i])->ref() = scalars[scalars_idx++];
    }
  }
  for (size_t i = 0; i < nvectors; i++) instances[0].vectorViews[variableNames[i + nvars]]->rebase(vectors[i]);

  if (it_ptr == nullptr) return exprtk_invalid_argument;

  const T *in_ptr = reinterpret_cast<const T *>(_iterator_vector);
  T *out_ptr = reinterpret_cast<T *>(_result);
  auto const in_end = in_ptr + iterator_len;
  auto &expression = instances[0].expression;
  for (; in_ptr < in_end; in_ptr++, out_ptr++) {
    *it_ptr = *in_ptr;
    *out_ptr = expression.value();
  }
  return exprtk_ok;
}

/**
 * Evaluate the expression for every element of a TypedArray
 * passing a scalar accumulator to every evaluation.
 * 
 * Evaluation and traversal happens entirely in C++ so this will be much
 * faster than calling `array.reduce(expr.eval)`.
 * 
 * All arrays must match the internal data type.
 * 
 * @param {TypedArray<T>} array for the expression to be iterated over
 * @param {string} iterator variable name
 * @param {string} accumulator variable name
 * @param {number} initializer for the accumulator
 * @param {...(number|TypedArray<T>)[]|Record<string, number|TypedArray<T>>} arguments of the function, iterator removed
 * @returns {number}
 * @memberof Expression
 *
 * @example
 * // n-power sum of an array
 * const sum = new Expression('a + pow(x, p)', ['a', 'x', 'p']);
 * 
 * // sumSq will be a scalar number
 * 
 * // These are equivalent
 * const sumSq = sum.reduce(array, 'x', 'a', 0, {'p': 2});
 * const sumSq = sum.reduce(array, 'x', 'a', 0, 2);
 *
 * sum.reduceAsync(array, 'x', {'a' : 0}, (e,r) => console.log(e, r));
 * const sumSq = await sum.reduceAsync(array, 'x', {'a' : 0}, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::reduce) {
  Napi::Env env = info.Env();

  ExprTkJob<T> job(this);

  std::vector<std::function<void(const ExpressionInstance<T> &)>> importers;

  if (
    info.Length() < 1 || !info[0].IsTypedArray() ||
    info[0].As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {

    Napi::TypeError::New(env, "first argument must be a " + std::string(NapiArrayType<T>::name))
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::TypedArray array = info[0].As<Napi::TypedArray>();
  T *input = reinterpret_cast<T *>(array.ArrayBuffer().Data());
  size_t len = array.ElementLength();

  if (info.Length() < 2 || !info[1].IsString()) {
    Napi::TypeError::New(env, "second argument must be the iterator variable name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const std::string iteratorName = info[1].As<Napi::String>().Utf8Value();
  auto iterator = instances[0].symbolTable.get_variable(iteratorName);
  if (iterator == nullptr) {
    Napi::TypeError::New(env, iteratorName + " is not a declared scalar variable").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 3 || !info[2].IsString()) {
    Napi::TypeError::New(env, "third argument must be the accumulator variable name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const std::string accuName = info[2].As<Napi::String>().Utf8Value();
  auto accu = instances[0].symbolTable.get_variable(accuName);
  if (accu == nullptr) {
    Napi::TypeError::New(env, accuName + " is not a declared scalar variable").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 4 || !info[3].IsNumber()) {
    Napi::TypeError::New(env, "fourth argument must be a number for the accumulator initial value")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  T accuInit = NapiArrayType<T>::CastFrom(info[3]);

  if (info.Length() > 4 && info[4].IsObject() && !info[4].IsTypedArray()) {
    importFromObject(env, job, info[4], importers);
  }

  if (info.Length() > 4 && (info[4].IsNumber() || info[4].IsTypedArray())) {
    size_t last = info.Length();
    if (async && last > 4 && info[last - 1].IsFunction()) last--;
    importFromArgumentsArray(env, job, info, 4, last, importers, {iteratorName, accuName});
  }

  if (instances[0].symbolTable.variable_count() + instances[0].symbolTable.vector_count() != importers.size() + 2) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  job.main = [importers, iteratorName, accuName, accuInit, input, len](const ExpressionInstance<T> &i) {
    auto iterator = i.symbolTable.get_variable(iteratorName);
    auto accu = i.symbolTable.get_variable(accuName);

    for (auto const &f : importers) f(i);
    accu->ref() = accuInit;
    T *it_ptr = &iterator->ref();
    T *accu_ptr = &accu->ref();
    T *input_end = input + len;
    auto &expression = i.expression;
    for (T *i = input; i < input_end; i++) {
      *it_ptr = *i;
      *accu_ptr = expression.value();
    }
    return accu->value();
  };
  job.rval = [env](T r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T>
exprtk_result Expression<T>::capi_reduce(
  const char *iterator_name,
  const size_t iterator_len,
  const void *_iterator_vector,
  const char *accumulator,
  const void *_scalars,
  void **_vectors,
  void *_result) {
  std::lock_guard<std::mutex> lock(asyncLock);
  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *it_ptr = nullptr;
  T *accu_ptr = nullptr;

  size_t nvars = instances[0].symbolTable.variable_count();
  size_t nvectors = instances[0].symbolTable.vector_count();

  int scalars_idx = 0;
  for (size_t i = 0; i < nvars; i++) {
    if (variableNames[i] == iterator_name) {
      it_ptr = &instances[0].symbolTable.get_variable(variableNames[i])->ref();
    } else if (variableNames[i] == accumulator) {
      accu_ptr = &instances[0].symbolTable.get_variable(variableNames[i])->ref();
    } else {
      instances[0].symbolTable.get_variable(variableNames[i])->ref() = scalars[scalars_idx++];
    }
  }
  for (size_t i = 0; i < nvectors; i++) instances[0].vectorViews[variableNames[nvars + i]]->rebase(vectors[i]);

  if (it_ptr == nullptr || accu_ptr == nullptr) return exprtk_invalid_argument;

  const T *in_ptr = reinterpret_cast<const T *>(_iterator_vector);
  T *out_ptr = reinterpret_cast<T *>(_result);
  auto const input_end = in_ptr + iterator_len;
  auto &expression = instances[0].expression;
  for (; in_ptr < input_end; in_ptr++) {
    *it_ptr = *in_ptr;
    *accu_ptr = expression.value();
  }
  *out_ptr = *accu_ptr;
  return exprtk_ok;
}

template <typename T> using NapiFromCaster_t = std::function<T(uint8_t *)>;
template <typename T> using NapiToCaster_t = std::function<void(uint8_t *, T)>;
template <typename T> struct symbolDesc {
  std::string name;
  napi_typedarray_type type;
  uint8_t *data;
  uint8_t storage[8];
  size_t elementSize;
  T *exprtk_var;
  NapiFromCaster_t<T> fromCaster;
};

// MSVC Linker has horrible bugs with templated variables
// but as long as they are local to the translation unit it should be ok

// Order is
// napi_int8_array
// napi_uint8_array
// napi_uint8_clamped_array
// napi_int16_array
// napi_uint16_array
// napi_int32_array
// napi_uint32_array
// napi_float32_array
// napi_float64_array
// napi_bigint64_array
// napi_biguint64_array
template <typename T>
static const NapiFromCaster_t<T> NapiFromCasters[] = {
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int8_t *>(data))); },
  [](uint8_t *data) { return *data; },
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int16_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<uint16_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int32_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<uint32_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<float *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<double *>(data))); },
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; }};

template <typename T>
static const NapiToCaster_t<T> NapiToCasters[] = {
  [](uint8_t *dst, T value) { *(reinterpret_cast<int8_t *>(dst)) = static_cast<int8_t>(value); },
  [](uint8_t *dst, T value) { *dst = static_cast<uint8_t>(value); },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { *(reinterpret_cast<int16_t *>(dst)) = static_cast<int16_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<uint16_t *>(dst)) = static_cast<uint16_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<int32_t *>(dst)) = static_cast<int32_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<uint32_t *>(dst)) = static_cast<uint32_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<float *>(dst)) = static_cast<float>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<double *>(dst)) = static_cast<double>(value); },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { throw "unsupported type"; }};

static const size_t NapiElementSize[] = {
  sizeof(int8_t),
  sizeof(uint8_t),
  0,
  sizeof(int16_t),
  sizeof(uint16_t),
  sizeof(int32_t),
  sizeof(uint32_t),
  sizeof(float),
  sizeof(double)};

/**
 * Generic vector operation with implicit traversal.
 * 
 * Supports automatic type conversions, multiple inputs and writing into a pre-existing array.
 *
 * @param {Record<string, number|TypedArray<T>>} arguments
 * @param {...(number|TypedArray<T>)[]|Record<string, number|TypedArray<T>>} arguments of the function, iterator removed
 * @returns {TypedArray<T>}
 * @memberof Expression
 *
 * @example
 * // Air density of humid air from relative humidity (phi), temperature (T) and pressure (P)
 * // rho = ( Pd * Md + Pv * Mv ) / ( R * (T + 273.15) // density (Avogadro's law)
 * // Pv = phi * Ps                                    // vapor pressure of water
 * // Ps = 6.1078 * 10 ^ (7.5 * T / (T + 237.3))       // saturation vapor pressure (Tetens' equation)
 * // Pd = P - Pv                                      // partial pressure of dry air
 * // R = 0.0831446                                    // universal gas constant
 * // Md = 0.0289652                                   // molar mass of water vapor
 * // Mv = 0.018016                                    // molar mass of dry air
 * // ( this is the weather science form of the equation and not the hard physics one with T in C° )
 * // phi, T and P are arbitrary TypedArrays of the same size
 * //
 * // Calculation uses Float64 internally
 * // Result is stored in Float32
 *
 * const R = 0.0831446;
 * const Md = 0.0289652;
 * const Mv = 0.018016;
 * // cwise()/cwiseAsync() accept and automatically convert all data types
 * const phi = new Float32Array([0, 0.2, 0.5, 0.9, 0.5]);
 * const P = new Uint16Array([1013, 1013, 1013, 1013, 995]);
 * const T = new Uint16Array([25, 25, 25, 25, 25]);
 * 
 * const density = new Float64Expression(
 *   'Pv := ( phi * 6.1078 * pow(10, (7.5 * T / (T + 237.3))) ); ' +  // compute Pv and store it
 *   '( (P - Pv) * Md + Pv * Mv ) / ( R * (T + 273.13) )',            // return expression
 *    ['P', 'T', 'phi', 'R', 'Md', 'Mv']
 * );
 * const result = new Float32Array(P.length);
 * 
 * // sync
 * density.cwise({phi, T, P, R, Md, Mv}, result);
 * 
 * // async
 * await density.cwiseAsync({phi, T, P, R, Md, Mv}, result);
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::cwise) {
  Napi::Env env = info.Env();

  ExprTkJob<T> job(this);

  if (info.Length() < 1 || !info[0].IsObject()) {

    Napi::TypeError::New(env, "first argument must be a an object containing the input values")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (instances[0].symbolTable.vector_count() > 0) {
    Napi::TypeError::New(env, "cwise()/cwiseAsync() are not compatible with vector arguments")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > 1 && !info[1].IsTypedArray() && (!async || !info[1].IsFunction())) {
    Napi::TypeError::New(env, "second argument must be a TypedArray or undefined").ThrowAsJavaScriptException();
    return env.Null();
  }

  bool typeConversionRequired = false;

  size_t len = 0;
  Napi::Object args = info[0].As<Napi::Object>();
  Napi::Array argNames = args.GetPropertyNames();
  std::vector<symbolDesc<T>> scalars, vectors;
  for (std::size_t i = 0; i < argNames.Length(); i++) {
    const std::string name = argNames.Get(i).As<Napi::String>().Utf8Value();
    Napi::Value value = args.Get(name);
    auto exprtk_ptr = instances[0].symbolTable.get_variable(name);
    if (exprtk_ptr == nullptr) {
      Napi::TypeError::New(env, name + " is not a declared scalar variable").ThrowAsJavaScriptException();
      return env.Null();
    }
    symbolDesc<T> current;
    current.exprtk_var = &exprtk_ptr->ref();

    if (value.IsNumber()) {
      current.name = name;
      current.type = NapiArrayType<T>::type;
      current.data = current.storage;
      *(reinterpret_cast<T *>(current.data)) = NapiArrayType<T>::CastFrom(value);
      scalars.push_back(current);
    } else if (value.IsTypedArray()) {
      Napi::TypedArray array = value.As<Napi::TypedArray>();
      if (len == 0)
        len = array.ElementLength();
      else if (len != array.ElementLength()) {
        Napi::TypeError::New(env, "all vectors must have the same number of elements").ThrowAsJavaScriptException();
        return env.Null();
      }
      current.name = name;
      current.type = array.TypedArrayType();
      current.data = reinterpret_cast<uint8_t *>(array.ArrayBuffer().Data());
      current.elementSize = array.ElementSize();
      current.fromCaster = NapiFromCasters<T>[current.type];
      if (current.type != NapiArrayType<T>::type) typeConversionRequired = true;
      vectors.push_back(current);
    } else {
      Napi::TypeError::New(env, name + " is not a number or a TypedArray").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  if (instances[0].symbolTable.variable_count() != scalars.size() + vectors.size()) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (len == 0) {
    Napi::TypeError::New(env, "at least one argument must be a non-zero length vector").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::TypedArray result;
  if (info.Length() > 1 && info[1].IsTypedArray()) {
    result = info[1].As<Napi::TypedArray>();
  } else {
    result = NapiArrayType<T>::New(env, len);
  }

  uint8_t *output = reinterpret_cast<uint8_t *>(result.ArrayBuffer().Data());
  size_t elementSize = result.ElementSize();
  napi_typedarray_type outputType = result.TypedArrayType();
  const NapiToCaster_t<T> toCaster = NapiToCasters<T>[outputType];
  if (outputType != NapiArrayType<T>::type) typeConversionRequired = true;

  auto persistent = std::make_shared<Napi::Reference<Napi::TypedArray>>(Napi::Persistent(result));

  if (typeConversionRequired) {
    job.main = [scalars, vectors, output, elementSize, len, toCaster](const ExpressionInstance<T> &i) mutable {
      for (auto &v : scalars) {
        v.exprtk_var = &i.symbolTable.get_variable(v.name)->ref();
        *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage));
      }
      for (auto &v : vectors) { v.exprtk_var = &i.symbolTable.get_variable(v.name)->ref(); }

      auto &expression = i.expression;
      uint8_t *output_end = output + len * elementSize;
      for (uint8_t *output_ptr = output; output_ptr < output_end; output_ptr += elementSize) {
        for (auto &v : vectors) {
          *v.exprtk_var = v.fromCaster(v.data);
          v.data += v.elementSize;
        }
        toCaster(output_ptr, expression.value());
      }
      return 0;
    };
  } else {
    job.main = [scalars, vectors, output, len](const ExpressionInstance<T> &i) mutable {
      for (auto &v : scalars) {
        v.exprtk_var = &i.symbolTable.get_variable(v.name)->ref();
        *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage));
      }
      for (auto &v : vectors) { v.exprtk_var = &i.symbolTable.get_variable(v.name)->ref(); }

      auto &expression = i.expression;
      T *output_end = reinterpret_cast<T *>(output) + len;
      for (T *output_ptr = reinterpret_cast<T *>(output); output_ptr < output_end; output_ptr++) {
        for (auto &v : vectors) {
          *v.exprtk_var = *(reinterpret_cast<T *>(v.data));
          v.data += v.elementSize;
        }
        *output_ptr = expression.value();
      }
      return 0;
    };
  }

  job.rval = [persistent](T r) { return persistent->Value(); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T>
exprtk_result
Expression<T>::capi_cwise(const size_t n_args, const exprtk_capi_cwise_arg *args, exprtk_capi_cwise_arg *result) {
  std::lock_guard<std::mutex> lock(asyncLock);

  bool typeConversionRequired = false;
  size_t len = 0;
  std::vector<symbolDesc<T>> scalars, vectors;

  if (instances[0].vectorViews.size() > 0)
    return exprtk_invalid_argument; // cwise is not (yet) compatible with vector arguments

  for (size_t i = 0; i < n_args; i++) {
    symbolDesc<T> current;
    auto exprtk_ptr = instances[0].symbolTable.get_variable(args[i].name);
    if (exprtk_ptr == nullptr) return exprtk_invalid_argument; // invalid variable name
    current.exprtk_var = &exprtk_ptr->ref();

    current.name = args[i].name;
    current.type = static_cast<napi_typedarray_type>(args[i].type);
    if (args[i].elements == 1) {
      current.data = current.storage;
      *(reinterpret_cast<T *>(current.data)) =
        NapiFromCasters<T>[current.type](reinterpret_cast<uint8_t *>(args[i].data));
      scalars.push_back(current);
    } else {
      if (len == 0)
        len = args[i].elements;
      else if (len != args[i].elements)
        return exprtk_invalid_argument; // all vectors must have the same number of elements
      current.data = reinterpret_cast<uint8_t *>(args[i].data);
      current.elementSize = NapiElementSize[args[i].type];
      current.fromCaster = NapiFromCasters<T>[current.type];
      if (current.type != NapiArrayType<T>::type) typeConversionRequired = true;
      vectors.push_back(current);
    }
  }

  if (instances[0].symbolTable.variable_count() != scalars.size() + vectors.size())
    return exprtk_invalid_argument; // wrong number of input arguments

  uint8_t *output = reinterpret_cast<uint8_t *>(result->data);
  size_t elementSize = NapiElementSize[result->type];
  auto const toCaster = NapiToCasters<T>[result->type];
  if (static_cast<napi_typedarray_type>(result->type) != NapiArrayType<T>::type) typeConversionRequired = true;

  if (typeConversionRequired) {
    for (auto const &v : scalars) { *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage)); }

    auto &expression = instances[0].expression;
    uint8_t *output_end = output + len * elementSize;
    for (uint8_t *output_ptr = output; output_ptr < output_end; output_ptr += elementSize) {
      for (auto &v : vectors) {
        *v.exprtk_var = v.fromCaster(v.data);
        v.data += v.elementSize;
      }
      toCaster(output_ptr, expression.value());
    }
  } else {
    for (auto const &v : scalars) { *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage)); }

    auto &expression = instances[0].expression;
    T *output_end = reinterpret_cast<T *>(output) + len;
    for (T *output_ptr = reinterpret_cast<T *>(output); output_ptr < output_end; output_ptr++) {
      for (auto &v : vectors) {
        *v.exprtk_var = *(reinterpret_cast<T *>(v.data));
        v.data += v.elementSize;
      }
      *output_ptr = expression.value();
    }
  }
  return exprtk_ok;
}

/**
 * Return the expression as a string
 *
 * @readonly
 * @kind member
 * @name expression
 * @instance
 * @memberof Expression
 * @type {string}
 */
template <typename T> Napi::Value Expression<T>::GetExpression(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  return Napi::String::New(env, expressionText);
}

/**
 * Return the type as a string
 *
 * @readonly
 * @kind member
 * @name type
 * @instance
 * @memberof Expression
 * @type {string}
 */
template <typename T> Napi::Value Expression<T>::GetType(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  return Napi::String::New(env, NapiArrayType<T>::name);
}

/**
 * Return the scalar arguments as an array
 *
 * @readonly
 * @kind member
 * @name scalars
 * @instance
 * @memberof Expression
 * @type {string[]}
 */
template <typename T> Napi::Value Expression<T>::GetScalars(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  Napi::Array scalars = Napi::Array::New(env);

  size_t i = 0;
  for (const auto &name : variableNames) {
    if (instances[0].symbolTable.get_variable(name)) { scalars.Set(i++, name); }
  }

  return scalars;
}

/**
 * Return the vector arguments as an object
 *
 * @readonly
 * @kind member
 * @name vectors
 * @instance
 * @memberof Expression
 * @type {Record<string, number[]>}
 */
template <typename T> Napi::Value Expression<T>::GetVectors(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  Napi::Object vectors = Napi::Object::New(env);

  for (const auto &name : variableNames) {
    auto vector = instances[0].symbolTable.get_vector(name);
    if (vector != nullptr) { vectors.Set(name, vector->size()); }
  }

  return vectors;
}

#define CALL_TYPED_EXPRESSION_METHOD(type, object, method, ...)                                                        \
  {                                                                                                                    \
    switch (static_cast<napi_typedarray_type>(type)) {                                                                 \
      case napi_uint8_array: return reinterpret_cast<Expression<uint8_t> *>(object)->method(__VA_ARGS__); break;       \
      case napi_int8_array: return reinterpret_cast<Expression<int8_t> *>(object)->method(__VA_ARGS__); break;         \
      case napi_uint16_array: return reinterpret_cast<Expression<uint16_t> *>(object)->method(__VA_ARGS__); break;     \
      case napi_int16_array: return reinterpret_cast<Expression<int16_t> *>(object)->method(__VA_ARGS__); break;       \
      case napi_uint32_array: return reinterpret_cast<Expression<uint32_t> *>(object)->method(__VA_ARGS__); break;     \
      case napi_int32_array: return reinterpret_cast<Expression<int32_t> *>(object)->method(__VA_ARGS__); break;       \
      case napi_float32_array: return reinterpret_cast<Expression<float> *>(object)->method(__VA_ARGS__); break;       \
      case napi_float64_array: return reinterpret_cast<Expression<double> *>(object)->method(__VA_ARGS__); break;      \
      default: return exprtk_invalid_argument;                                                                         \
    }                                                                                                                  \
  }

extern "C" {
exprtk_result entry_capi_eval(exprtk_expression *expression, const void *scalars, void **vectors, void *result) {
  CALL_TYPED_EXPRESSION_METHOD(expression->type, expression->_descriptor_, capi_eval, scalars, vectors, result);
}

exprtk_result entry_capi_map(
  exprtk_expression *expression,
  const char *iterator_name,
  const size_t iterator_len,
  const void *iterator_vector,
  const void *scalars,
  void **vectors,
  void *result) {

  CALL_TYPED_EXPRESSION_METHOD(
    expression->type,
    expression->_descriptor_,
    capi_map,
    iterator_name,
    iterator_len,
    iterator_vector,
    scalars,
    vectors,
    result);
}

exprtk_result entry_capi_reduce(
  exprtk_expression *expression,
  const char *iterator_name,
  const size_t iterator_len,
  const void *iterator_vector,
  const char *accumulator,
  const void *scalars,
  void **vectors,
  void *result) {
  CALL_TYPED_EXPRESSION_METHOD(
    expression->type,
    expression->_descriptor_,
    capi_reduce,
    iterator_name,
    iterator_len,
    iterator_vector,
    accumulator,
    scalars,
    vectors,
    result);
}

exprtk_result entry_capi_cwise(
  exprtk_expression *expression,
  const size_t n_args,
  const exprtk_capi_cwise_arg *args,
  exprtk_capi_cwise_arg *result) {
  CALL_TYPED_EXPRESSION_METHOD(expression->type, expression->_descriptor_, capi_cwise, n_args, args, result);
}
} // extern "C"

template <typename T> Napi::Value Expression<T>::GetCAPI(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (capiDescriptor != nullptr) return capiDescriptor->Value();

  auto &symbolTable = instances[0].symbolTable;
  size_t size = sizeof(exprtk_expression) + symbolTable.variable_count() * sizeof(char *) +
    symbolTable.vector_count() * sizeof(exprtk_capi_vector);

  Napi::ArrayBuffer result = Napi::ArrayBuffer::New(env, size);
  capiDescriptor = std::make_shared<Napi::Reference<Napi::ArrayBuffer>>(Napi::Persistent(result));
  exprtk_expression *desc = reinterpret_cast<exprtk_expression *>(result.Data());

  desc->magic = EXPRTK_JS_CAPI_MAGIC;
  desc->_descriptor_ = this;
  desc->expression = expressionText.c_str();
  desc->type = static_cast<napi_compatible_type>(NapiArrayType<T>::type);

  desc->scalars_len = symbolTable.variable_count();
  desc->vectors_len = symbolTable.vector_count();

  desc->scalars = reinterpret_cast<const char **>(reinterpret_cast<uint8_t *>(desc) + sizeof(exprtk_expression));
  for (size_t i = 0; i < symbolTable.variable_count(); i++) { desc->scalars[i] = variableNames[i].c_str(); }

  desc->vectors = reinterpret_cast<exprtk_capi_vector *>(
    reinterpret_cast<uint8_t *>(desc->scalars) + sizeof(char *) * symbolTable.variable_count());
  for (size_t i = 0; i < symbolTable.vector_count(); i++) {
    desc->vectors[i].name = variableNames[i + symbolTable.variable_count()].c_str();
    desc->vectors[i].elements = symbolTable.get_vector(variableNames[i + symbolTable.variable_count()])->size();
  }

  desc->eval = entry_capi_eval;
  desc->map = entry_capi_map;
  desc->reduce = entry_capi_reduce;
  desc->cwise = entry_capi_cwise;

  return result;
}

/**
 * Get/set the maximum allowed parallel instances for this Expression
 *
 * @readonly
 * @kind member
 * @name maxParallel
 * @instance
 * @memberof Expression
 * @type {number}
 */
template <typename T> Napi::Value Expression<T>::GetMaxParallel(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  return Napi::Number::New(env, maxParallel);
}

template <typename T> void Expression<T>::SetMaxParallel(const Napi::CallbackInfo &info, const Napi::Value &value) {
  Napi::Env env = info.Env();

  if (value.IsEmpty() || !value.IsNumber()) {
    Napi::TypeError::New(env, "value must be a number").ThrowAsJavaScriptException();
    return;
  }

  size_t newMax = value.ToNumber().Uint32Value();
  if (newMax > std::thread::hardware_concurrency()) {
    Napi::TypeError::New(
      env,
      "maximum instances is currently limited to the number of hardware cores: " +
        std::to_string(std::thread::hardware_concurrency()))
      .ThrowAsJavaScriptException();
    return;
  }
  maxParallel = newMax;
}

template <typename T> Napi::Function Expression<T>::GetClass(Napi::Env env) {
  napi_property_attributes props =
    static_cast<napi_property_attributes>(napi_writable | napi_configurable | napi_enumerable);
  napi_property_attributes hidden = static_cast<napi_property_attributes>(napi_configurable);
  static const std::string className = std::string(NapiArrayType<T>::name) + "Expression";
  return Expression<T>::DefineClass(
    env,
    className.c_str(),
    {Expression<T>::InstanceAccessor("expression", &Expression<T>::GetExpression, nullptr),
     Expression<T>::InstanceAccessor("scalars", &Expression<T>::GetScalars, nullptr),
     Expression<T>::InstanceAccessor("vectors", &Expression<T>::GetVectors, nullptr),
     Expression<T>::InstanceAccessor("type", &Expression<T>::GetType, nullptr),
     Expression<T>::InstanceAccessor("_CAPI_", &Expression<T>::GetCAPI, nullptr, hidden),
     Expression<T>::InstanceAccessor("maxParallel", &Expression<T>::GetMaxParallel, &Expression<T>::SetMaxParallel),
     ASYNCABLE_INSTANCE_METHOD(Expression<T>, eval, props),
     ASYNCABLE_INSTANCE_METHOD(Expression<T>, map, props),
     ASYNCABLE_INSTANCE_METHOD(Expression<T>, reduce, props),
     ASYNCABLE_INSTANCE_METHOD(Expression<T>, cwise, props)});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  initAsyncWorkers();
#ifndef EXPRTK_DISABLE_INT_TYPES
  exports.Set(Napi::String::New(env, NapiArrayType<int8_t>::name), Expression<int8_t>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<uint8_t>::name), Expression<uint8_t>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<int16_t>::name), Expression<int16_t>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<uint16_t>::name), Expression<uint16_t>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<int32_t>::name), Expression<int32_t>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<uint32_t>::name), Expression<uint32_t>::GetClass(env));
#endif
  exports.Set(Napi::String::New(env, NapiArrayType<float>::name), Expression<float>::GetClass(env));
  exports.Set(Napi::String::New(env, NapiArrayType<double>::name), Expression<double>::GetClass(env));
  return exports;
}

NODE_API_MODULE(exprtkjs, Init)
