#include "expression.h"

#include <memory>
#include <cstdlib>

namespace exprtk_js {

/**
 * @class Expression
 * @param {string} expression function
 * @param {string[]} [variables] An array containing all the scalar variables' names, will be determined automatically if omitted, however order won't be guaranteed, scalars are passed by value
 * @param {Record<string, number>} [vectors] An object containing all the vector variables' names and their sizes, vector size must be known at compilation (object construction), vectors are passed by reference and can be modified by the ExprTk expression
 * @returns {Expression}
 * 
 * The `Expression` represents an expression compiled to an AST from a string. Expressions come in different flavors depending on the internal type used.
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

size_t ExpressionMaxParallel = std::thread::hardware_concurrency();
template <typename T>
Expression<T>::Expression(const Napi::CallbackInfo &info)
  : Napi::ObjectWrap<Expression<T>>::ObjectWrap(info),
    maxParallel(ExpressionMaxParallel),
    maxActive(1),
    currentActive(0),
    instances(ExpressionMaxParallel),
    capiDescriptor(nullptr) {
  Napi::Env env = info.Env();

  instances[0].isInit = false;
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

      size_t size = value.ToNumber().Int64Value();
      instances[0].vectorViews[name] = std::make_unique<exprtk::vector_view<T>>(nullptr, size);

      if (!instances[0].symbolTable.add_vector(name, *instances[0].vectorViews[name])) {
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
  instancesIdle.push_back(&instances[0]);
  for (size_t i = 1; i < ExpressionMaxParallel; i++) {
    instances[i].isInit = false;
    instancesIdle.push_back(&instances[i]);
  }
}

template <typename T> Expression<T>::~Expression() {
  std::lock_guard<std::mutex> lock(asyncLock);
  if (instances[0].isInit) {
    size_t free = 0;
    for (; !instancesIdle.empty(); instancesIdle.pop_front(), free++);
    if (free != instances.size())
      fprintf(
        stderr,
        "GC waiting on a background evaluation of an Expression object, event loop blocked. "
        "If you are using only the JS interface, this is a bug in ExprTk.js. "
        "If you are using the C/C++ API, you must always protect Expression objects from the GC "
        "by obtaining a persistent object reference. \n");
  }
  for (auto &i : instances) {
    if (i.isInit) {
      for (auto const &v : i.vectorViews) {
        // exprtk will sometimes try to free this pointer
        // on object destruction even if it never allocated it
        v.second->rebase((T *)nullptr);
      }
    }
  }
}

template <typename T> void Expression<T>::compileInstance(ExpressionInstance<T> *i) {
  if (i->isInit) return;
  for (auto const &name : variableNames) {
    if (instances[0].symbolTable.get_variable(name))
      i->symbolTable.create_variable(name);
    else {
      auto vector = instances[0].symbolTable.get_vector(name);
      auto size = vector->size();
      i->vectorViews[name] = std::make_unique<exprtk::vector_view<T>>(nullptr, size);
      i->symbolTable.add_vector(name, *i->vectorViews[name]);
    }
  }
  i->isInit = true;
  i->expression.register_symbol_table(i->symbolTable);
  std::lock_guard<std::mutex> lock(parserMutex);
  maxActive++;
  parser().compile(expressionText, i->expression);
}

template <typename T> static inline T *GetTypedArrayPtr(const Napi::TypedArray &array) {
  return reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(array.ArrayBuffer().Data()) + array.ByteOffset());
}

/**
 * Evaluate the expression.
 * 
 * All arrays must match the internal data type.
 *
 * @instance
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

  Job<T> job(this);

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

  job.main = [importers](const ExpressionInstance<T> &i, size_t) {
    for (auto const &f : importers) f(i);
    T r = i.expression.value();
    if (i.expression.results().count()) { throw "explicit return values are not supported"; }
    return r;
  };
  job.rval = [env](T r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T> exprtk_result Expression<T>::capi_eval(const void *_scalars, void **_vectors, void *_result) {
  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *result = reinterpret_cast<T *>(_result);

  InstanceGuard<T> instance(this);

  size_t nvars = instance()->symbolTable.variable_count();
  size_t nvectors = instance()->symbolTable.vector_count();
  for (size_t i = 0; i < nvars; i++) instance()->symbolTable.get_variable(variableNames[i])->ref() = scalars[i];
  for (size_t i = 0; i < nvectors; i++) instance()->vectorViews[variableNames[i + nvars]]->rebase(vectors[i]);
  *result = instance()->expression.value();

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
 * If target is specified, it will write the data into a preallocated array.
 * This can be used when multiple operations are chained to avoid reallocating a new array at every step.
 * Otherwise it will return a new array.
 *
 * @instance
 * @param {TypedArray<T>} [threads] number of threads to use, 1 if not specified
 * @param {TypedArray<T>} [target] array in which the data is to be written, will allocate a new array if none is specified
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
 * // In a preallocated array
 * const r = new array.constructor(array.length);
 * // These two are equivalent
 * expr.map(r, array, 'x', 0, 1000);
 * expr.map(r, array, 'x', {f: 0, c: 0});
 *
 * expr.mapAsync(r, array, 'x', 0, 1000, (e,r) => console.log(e, r));
 * expr.mapAsync(r, array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));
 * 
 * // In a new array
 * // r1/r2 will be TypedArray's of the same type
 * const r1 = expr.map(array, 'x', 0, 1000);
 * const r2 = expr.map(array, 'x', {f: 0, c: 0});
 *
 * expr.mapAsync(array, 'x', 0, 1000, (e,r) => console.log(e, r));
 * expr.mapAsync(array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));
 * 
 * // Using multiple (4) parallel threads (OpenMP-style parallelism)
 * const r1 = expr.map(4, array, 'x', 0, 1000);
 * const r2 = await expr.mapAsync(4, array, 'x', {f: 0, c: 0});
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::map) {
  Napi::Env env = info.Env();

  Job<T> job(this);

  std::vector<std::function<void(const ExpressionInstance<T> &)>> importers;

  size_t arg = 0;
  if (info.Length() > arg + 1 && info[arg].IsNumber()) {
    job.joblets = static_cast<size_t>(info[0].ToNumber().Uint32Value());
    arg++;
    if (job.joblets > maxParallel) {
      Napi::TypeError::New(env, "maximum threads must not exceed maxParallel = " + std::to_string(maxParallel))
        .ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  Napi::TypedArray result;
  if (info.Length() > arg + 1 && info[arg + 1].IsTypedArray()) {
    // The caller passed a preallocated array
    result = info[arg].As<Napi::TypedArray>();
    if (result.TypedArrayType() != NapiArrayType<T>::type) {
      Napi::TypeError::New(env, "target array must be a " + std::string(NapiArrayType<T>::name) + "Array")
        .ThrowAsJavaScriptException();
      return env.Null();
    }
    arg++;
  }

  if (
    info.Length() < arg + 1 || !info[arg].IsTypedArray() ||
    info[arg].As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {

    Napi::TypeError::New(env, "array argument must be a " + std::string(NapiArrayType<T>::name) + "Array")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::TypedArray array = info[arg++].As<Napi::TypedArray>();
  T *input = GetTypedArrayPtr<T>(array);
  size_t lenTotal = array.ElementLength();
  if (result.IsEmpty()) { result = NapiArrayType<T>::New(env, lenTotal); }

  if (result.ElementLength() != array.ElementLength()) {
    Napi::TypeError::New(env, "both arrays must have the same size").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < arg + 1 || !info[arg].IsString()) {
    Napi::TypeError::New(env, "invalid iterator variable name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const std::string iteratorName = info[arg++].As<Napi::String>().Utf8Value();
  auto iterator = instances[0].symbolTable.get_variable(iteratorName);
  if (iterator == nullptr) {
    Napi::TypeError::New(env, iteratorName + " is not a declared scalar variable").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > arg && info[arg].IsObject() && !info[arg].IsTypedArray()) {
    importFromObject(env, job, info[arg], importers);
  }

  if (info.Length() > arg && (info[arg].IsNumber() || info[arg].IsTypedArray())) {
    size_t last = info.Length();
    if (async && last > 2 && info[last - 1].IsFunction()) last--;
    importFromArgumentsArray(env, job, info, arg, last, importers, {iteratorName});
  }

  if (instances[0].symbolTable.variable_count() + instances[0].symbolTable.vector_count() != importers.size() + 1) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  T *output = GetTypedArrayPtr<T>(result);

  // integer division ceiling
  size_t lenPerJoblet = (lenTotal + job.joblets - 1) / job.joblets;

  // this should have been an unique_ptr
  // but std::function is not compatible with move semantics
  auto persistent = std::make_shared<Napi::Reference<Napi::TypedArray>>(Napi::Persistent(result));

  job.main =
    [importers, iteratorName, input, output, lenTotal, lenPerJoblet](const ExpressionInstance<T> &i, size_t id) {
      for (auto const &f : importers) f(i);
      auto iterator = i.symbolTable.get_variable(iteratorName);

      T *it_ptr = &iterator->ref();
      T *in_ptr = input + id * lenPerJoblet;
      T *out_ptr = output + id * lenPerJoblet;
      const T *in_end = input + std::min(lenTotal, (id + 1) * lenPerJoblet);
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
  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *it_ptr = nullptr;

  InstanceGuard<T> instance(this);

  size_t nvars = instance()->symbolTable.variable_count();
  size_t nvectors = instance()->symbolTable.vector_count();

  size_t scalars_idx = 0;
  for (size_t i = 0; i < nvars; i++) {
    if (variableNames[i] == iterator_name) {
      it_ptr = &instance()->symbolTable.get_variable(variableNames[i])->ref();
    } else {
      instance()->symbolTable.get_variable(variableNames[i])->ref() = scalars[scalars_idx++];
    }
  }
  for (size_t i = 0; i < nvectors; i++) instance()->vectorViews[variableNames[i + nvars]]->rebase(vectors[i]);

  if (it_ptr == nullptr) return exprtk_invalid_argument;

  const T *in_ptr = reinterpret_cast<const T *>(_iterator_vector);
  T *out_ptr = reinterpret_cast<T *>(_result);
  auto const in_end = in_ptr + iterator_len;
  auto &expression = instance()->expression;
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
 * @instance
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

  Job<T> job(this);

  std::vector<std::function<void(const ExpressionInstance<T> &)>> importers;

  if (
    info.Length() < 1 || !info[0].IsTypedArray() ||
    info[0].As<Napi::TypedArray>().TypedArrayType() != NapiArrayType<T>::type) {

    Napi::TypeError::New(env, "first argument must be a " + std::string(NapiArrayType<T>::name))
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::TypedArray array = info[0].As<Napi::TypedArray>();
  T *input = GetTypedArrayPtr<T>(array);
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

  job.main = [importers, iteratorName, accuName, accuInit, input, len](const ExpressionInstance<T> &i, size_t) {
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
  const T *scalars = reinterpret_cast<const T *>(_scalars);
  T **vectors = reinterpret_cast<T **>(_vectors);
  T *it_ptr = nullptr;
  T *accu_ptr = nullptr;

  InstanceGuard<T> instance(this);

  size_t nvars = instance()->symbolTable.variable_count();
  size_t nvectors = instance()->symbolTable.vector_count();

  int scalars_idx = 0;
  for (size_t i = 0; i < nvars; i++) {
    if (variableNames[i] == iterator_name) {
      it_ptr = &instance()->symbolTable.get_variable(variableNames[i])->ref();
    } else if (variableNames[i] == accumulator) {
      accu_ptr = &instance()->symbolTable.get_variable(variableNames[i])->ref();
    } else {
      instance()->symbolTable.get_variable(variableNames[i])->ref() = scalars[scalars_idx++];
    }
  }
  for (size_t i = 0; i < nvectors; i++) instance()->vectorViews[variableNames[nvars + i]]->rebase(vectors[i]);

  if (it_ptr == nullptr || accu_ptr == nullptr) return exprtk_invalid_argument;

  const T *in_ptr = reinterpret_cast<const T *>(_iterator_vector);
  T *out_ptr = reinterpret_cast<T *>(_result);
  auto const input_end = in_ptr + iterator_len;
  auto &expression = instance()->expression;
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

  // for ndarrays only
  int64_t offset;
  std::shared_ptr<size_t[]> index;
  std::shared_ptr<int32_t[]> stride;
  int32_t smallestStride;
  uint8_t *data_ptr;
  uint8_t *data_end;
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
#ifndef EXPRTK_DISABLE_INT_TYPES
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int8_t *>(data))); },
  [](uint8_t *data) { return *data; },
#else
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; },
#endif
  [](uint8_t *data) -> T { throw "unsupported type"; },
#ifndef EXPRTK_DISABLE_INT_TYPES
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int16_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<uint16_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<int32_t *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<uint32_t *>(data))); },
#else
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; },
#endif
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<float *>(data))); },
  [](uint8_t *data) { return static_cast<T>(*(reinterpret_cast<double *>(data))); },
  [](uint8_t *data) -> T { throw "unsupported type"; },
  [](uint8_t *data) -> T { throw "unsupported type"; }};

template <typename T>
static const NapiToCaster_t<T> NapiToCasters[] = {
#ifndef EXPRTK_DISABLE_INT_TYPES
  [](uint8_t *dst, T value) { *(reinterpret_cast<int8_t *>(dst)) = static_cast<int8_t>(value); },
  [](uint8_t *dst, T value) { *dst = static_cast<uint8_t>(value); },
#else
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
#endif
  [](uint8_t *dst, T value) { throw "unsupported type"; },
#ifndef EXPRTK_DISABLE_INT_TYPES
  [](uint8_t *dst, T value) { *(reinterpret_cast<int16_t *>(dst)) = static_cast<int16_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<uint16_t *>(dst)) = static_cast<uint16_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<int32_t *>(dst)) = static_cast<int32_t>(value); },
  [](uint8_t *dst, T value) { *(reinterpret_cast<uint32_t *>(dst)) = static_cast<uint32_t>(value); },
#else
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
  [](uint8_t *dst, T value) { throw "unsupported type"; },
#endif
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
 * Supports automatic type conversions, multiple inputs, strided N-dimensional arrays and writing into a pre-existing array.
 * 
 * If using N-dimensional arrays, all arrays must have the same shape. The result is always in positive row-major order.
 * When mixing linear vectors and N-dimensional arrays, the linear vectors are considered to be in positive row-major order
 * in relation to the N-dimensional arrays.
 *
 * @instance
 * @param {number} [threads]
 * @param {Record<string, number|TypedArray<any> | ndarray.NdArray<any> | stdlib.ndarray>} arguments
 * @param {TypedArray<T>} [target]
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
 * // sync multithreaded
 * density.cwise(os.cpus().length, {phi, T, P, R, Md, Mv}, result);
 *
 * // async
 * await density.cwiseAsync({phi, T, P, R, Md, Mv}, result);
 * 
 * // async multithreaded
 * await density.cwiseAsync(os.cpus().length, {phi, T, P, R, Md, Mv}, result);
 */
ASYNCABLE_DEFINE(template <typename T>, Expression<T>::cwise) {
  Napi::Env env = info.Env();

  Job<T> job(this);

  size_t arg = 0;
  if (info.Length() > arg + 1 && info[arg].IsNumber()) {
    job.joblets = static_cast<size_t>(info[0].ToNumber().Uint32Value());
    arg++;
    if (job.joblets > maxParallel) {
      Napi::TypeError::New(env, "maximum threads must not exceed maxParallel = " + std::to_string(maxParallel))
        .ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  if (info.Length() < 1 || !info[arg].IsObject()) {
    Napi::TypeError::New(env, "first argument must be a an object containing the input values")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Object args = info[arg].As<Napi::Object>();
  arg++;

  if (instances[0].symbolTable.vector_count() > 0) {
    Napi::TypeError::New(env, "cwise()/cwiseAsync() are not compatible with vector arguments")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= arg + 1 && !info[arg].IsTypedArray() && (!async || !info[arg].IsFunction())) {
    Napi::TypeError::New(env, "last argument must be a TypedArray or undefined").ThrowAsJavaScriptException();
    return env.Null();
  }

  bool typeConversionRequired = false;

  size_t len = 0, dims = 0;
  Napi::Array argNames = args.GetPropertyNames();
  std::vector<symbolDesc<T>> scalars, vectors, ndarrays;
  std::shared_ptr<size_t[]> shape;
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

    size_t thisDims;
    std::shared_ptr<size_t[]> thisShape;
    if (value.IsNumber()) {
      current.name = name;
      current.type = NapiArrayType<T>::type;
      current.data = current.storage;
      *(reinterpret_cast<T *>(current.data)) = NapiArrayType<T>::CastFrom(value);
      scalars.push_back(current);
    } else if (value.IsTypedArray() || ImportStridedArray(value, thisDims, current.offset, thisShape, current.stride)) {
      Napi::TypedArray array;
      size_t thisLen;

      if (value.IsTypedArray()) {
        array = value.As<Napi::TypedArray>();
        thisLen = array.ElementLength();
      } else {
        array = StridedArrayBuffer(value.ToObject());
        thisLen = 1;
        if (dims == 0) {
          dims = thisDims;
          shape = thisShape;
        }
        if (dims != thisDims) {
          Napi::TypeError::New(env, "all strided arrays must have the same number of dimensions")
            .ThrowAsJavaScriptException();
          return env.Null();
        }
        if (!ArraysEqual(thisShape, shape, dims)) {
          Napi::TypeError::New(env, "all strided arrays must have the same shape").ThrowAsJavaScriptException();
          return env.Null();
        }
        thisLen = StridedLength(thisShape, dims);
      }

      if (len == 0)
        len = thisLen;
      else if (len != thisLen) {
        Napi::TypeError::New(env, "all vectors must have the same number of elements").ThrowAsJavaScriptException();
        return env.Null();
      }
      current.name = name;
      current.type = array.TypedArrayType();
      current.data = GetTypedArrayPtr<uint8_t>(array);
      current.elementSize = array.ElementSize();
      current.fromCaster = NapiFromCasters<T>[current.type];
      if (current.type != NapiArrayType<T>::type) typeConversionRequired = true;
      if (value.IsTypedArray()) {
        vectors.push_back(current);
      } else {
        current.data += current.offset * current.elementSize;
        current.smallestStride = current.stride[dims - 1] * current.elementSize;
        ndarrays.push_back(current);
      }
    } else {
      Napi::TypeError::New(env, name + " is not a number or a TypedArray").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  if (instances[0].symbolTable.variable_count() != scalars.size() + vectors.size() + ndarrays.size()) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (len == 0) {
    Napi::TypeError::New(env, "at least one argument must be a non-zero length vector").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::TypedArray result;
  if (info.Length() >= arg + 1 && info[arg].IsTypedArray()) {
    result = info[arg].As<Napi::TypedArray>();
    if (result.ElementLength() < len) {
      Napi::TypeError::New(env, "target array cannot hold the result").ThrowAsJavaScriptException();
      return env.Null();
    }
  } else {
    result = NapiArrayType<T>::New(env, len);
  }

  uint8_t *output = GetTypedArrayPtr<uint8_t>(result);
  size_t elementSize = result.ElementSize();
  napi_typedarray_type outputType = result.TypedArrayType();
  const NapiToCaster_t<T> toCaster = NapiToCasters<T>[outputType];
  if (outputType != NapiArrayType<T>::type) typeConversionRequired = true;

  auto persistent = std::make_shared<Napi::Reference<Napi::TypedArray>>(Napi::Persistent(result));

  // integer division ceiling
  size_t lenPerJoblet = (len + job.joblets - 1) / job.joblets;

  std::shared_ptr<int32_t[]> rowMajorStride;
  if (ndarrays.size() > 0) {
    rowMajorStride = std::shared_ptr<int32_t[]>(new int32_t[dims]);
    int32_t stride = 1;
    for (int32_t d = dims - 1; d >= 0; d--) {
      rowMajorStride[d] = stride;
      stride *= shape[d];
    }
  }
  job.main = [scalars,
              vectors,
              ndarrays,
              output,
              elementSize,
              len,
              toCaster,
              lenPerJoblet,
              dims,
              typeConversionRequired,
              shape,
              rowMajorStride](const ExpressionInstance<T> &i, size_t id) {
    size_t scalarsNumber = scalars.size();
    size_t vectorsNumber = vectors.size();
    size_t ndArraysNumber = ndarrays.size();

    symbolDesc<T> *localScalars = new symbolDesc<T>[scalarsNumber];
    for (size_t j = 0; j < scalarsNumber; j++) localScalars[j] = scalars[j];
    symbolDesc<T> *localVectors = new symbolDesc<T>[vectorsNumber];
    for (size_t j = 0; j < vectorsNumber; j++) localVectors[j] = vectors[j];
    symbolDesc<T> *localNDArrays = new symbolDesc<T>[ndArraysNumber];
    for (size_t j = 0; j < ndArraysNumber; j++) localNDArrays[j] = ndarrays[j];

    auto *localScalarsEnd = localScalars + scalarsNumber;
    auto *localVectorsEnd = localVectors + vectorsNumber;
    auto *localNDArraysEnd = localNDArrays + ndArraysNumber;

    for (auto *v = localScalars; v != localScalarsEnd; v++) {
      v->exprtk_var = &i.symbolTable.get_variable(v->name)->ref();
      *v->exprtk_var = *(reinterpret_cast<const T *>(v->storage));
    }
    for (auto *v = localVectors; v != localVectorsEnd; v++) {
      v->exprtk_var = &i.symbolTable.get_variable(v->name)->ref();
      v->data = v->data + id * lenPerJoblet * v->elementSize;
    }

    // Output is (for now) always positive-row-major
    for (auto *v = localNDArrays; v != localNDArraysEnd; v++) {
      v->exprtk_var = &i.symbolTable.get_variable(v->name)->ref();
      v->index = std::shared_ptr<size_t[]>(new size_t[dims]);
      // Offset (linear index) is determined by the joblet id
      v->offset = id * lenPerJoblet;
      // Convert the offset to subscripts for a rowMajorStide
      // to find the starting position
      GetStridedIndex(v->offset, v->index, dims, shape, rowMajorStride);
      // and then convert the subscripts to index for each ndarray separately
      GetLinearOffset(v->offset, v->index, dims, shape, v->stride);

      // Shortcut to allow iterating over the last dimension with a normal linear loop
      v->data_ptr = v->data + v->offset * v->elementSize;
      v->data_end = v->data_ptr + (shape[dims - 1] - v->index[dims - 1]) * v->stride[dims - 1] * v->elementSize;
    }

    auto &expression = i.expression;

    // The time critical loops
    if (typeConversionRequired && ndArraysNumber > 0) {
      // The full loop
      uint8_t *output_end = output + std::min((id + 1) * lenPerJoblet, len) * elementSize;
      for (uint8_t *output_ptr = output + id * lenPerJoblet * elementSize; output_ptr < output_end;
           output_ptr += elementSize) {
        for (auto *v = localVectors; v != localVectorsEnd; v++) {
          *v->exprtk_var = v->fromCaster(v->data);
          v->data += v->elementSize;
        }
        for (auto *v = localNDArrays; v != localNDArraysEnd; v++) {
          *v->exprtk_var = v->fromCaster(v->data_ptr);
          v->data_ptr += v->smallestStride;
          if (v->data_ptr == v->data_end) {
            v->index[dims - 1] = shape[dims - 1] - 1;
            IncrementStridedIndex(v->index, v->data, &v->data_ptr, v->elementSize, dims, shape, v->stride);
            v->data_end = v->data_ptr + (shape[dims - 1] - v->index[dims - 1]) * v->stride[dims - 1] * v->elementSize;
          }
        }
        toCaster(output_ptr, expression.value());
      }
    } else if (ndArraysNumber > 0) {
      // With ndarrays without type conversion
      T *output_end = reinterpret_cast<T *>(output) + std::min((id + 1) * lenPerJoblet, len);
      for (T *output_ptr = reinterpret_cast<T *>(output) + id * lenPerJoblet; output_ptr < output_end; output_ptr++) {
        for (auto *v = localVectors; v != localVectorsEnd; v++) {
          *v->exprtk_var = *(reinterpret_cast<T *>(v->data));
          v->data += v->elementSize;
        }
        for (auto *v = localNDArrays; v != localNDArraysEnd; v++) {
          *v->exprtk_var = *(reinterpret_cast<T *>(v->data_ptr));
          v->data_ptr += v->smallestStride;
          if (v->data_ptr == v->data_end) {
            v->index[dims - 1] = shape[dims - 1] - 1;
            IncrementStridedIndex(v->index, v->data, &v->data_ptr, v->elementSize, dims, shape, v->stride);
            v->data_end = v->data_ptr + (shape[dims - 1] - v->index[dims - 1]) * v->stride[dims - 1] * v->elementSize;
          }
        }
        *output_ptr = expression.value();
      }
    } else if (typeConversionRequired) {
      // Without ndarrays with type conversion
      uint8_t *output_end = output + std::min((id + 1) * lenPerJoblet, len) * elementSize;
      for (uint8_t *output_ptr = output + id * lenPerJoblet * elementSize; output_ptr < output_end;
           output_ptr += elementSize) {
        for (auto *v = localVectors; v != localVectorsEnd; v++) {
          *v->exprtk_var = v->fromCaster(v->data);
          v->data += v->elementSize;
        }
        toCaster(output_ptr, expression.value());
      }
    } else {
      // The fast simple loop
      T *output_end = reinterpret_cast<T *>(output) + std::min((id + 1) * lenPerJoblet, len);
      for (T *output_ptr = reinterpret_cast<T *>(output) + id * lenPerJoblet; output_ptr < output_end; output_ptr++) {
        for (auto *v = localVectors; v != localVectorsEnd; v++) {
          *v->exprtk_var = *(reinterpret_cast<T *>(v->data));
          v->data += v->elementSize;
        }
        *output_ptr = expression.value();
      }
    }
    delete[] localScalars;
    delete[] localVectors;
    delete[] localNDArrays;
    return 0;
  };

  job.rval = [persistent](T r) { return persistent->Value(); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T>
exprtk_result
Expression<T>::capi_cwise(const size_t n_args, const exprtk_capi_cwise_arg *args, exprtk_capi_cwise_arg *result) {
  bool typeConversionRequired = false;
  size_t len = 0;
  std::vector<symbolDesc<T>> scalars, vectors;

  InstanceGuard<T> instance(this);

  if (instance()->vectorViews.size() > 0)
    return exprtk_invalid_argument; // cwise is not (yet) compatible with vector arguments

  for (size_t i = 0; i < n_args; i++) {
    symbolDesc<T> current;
    auto exprtk_ptr = instance()->symbolTable.get_variable(args[i].name);
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

  if (instance()->symbolTable.variable_count() != scalars.size() + vectors.size())
    return exprtk_invalid_argument; // wrong number of input arguments

  uint8_t *output = reinterpret_cast<uint8_t *>(result->data);
  size_t elementSize = NapiElementSize[result->type];
  auto const toCaster = NapiToCasters<T>[result->type];
  if (static_cast<napi_typedarray_type>(result->type) != NapiArrayType<T>::type) typeConversionRequired = true;

  if (typeConversionRequired) {
    for (auto const &v : scalars) { *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage)); }

    auto &expression = instance()->expression;
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

    auto &expression = instance()->expression;
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

/**
 * Return the data type constructor
 *
 * @readonly
 * @kind member
 * @name allocator
 * @instance
 * @memberof Expression
 * @type {TypedArrayConstructor}
 */

/**
 * Return the data type constructor
 *
 * @readonly
 * @kind member
 * @name allocator
 * @static
 * @memberof Expression
 * @type {TypedArrayConstructor}
 */

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

#ifndef EXPRTK_DISABLE_INT_TYPES
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
#else
#define CALL_TYPED_EXPRESSION_METHOD(type, object, method, ...)                                                        \
  {                                                                                                                    \
    switch (static_cast<napi_typedarray_type>(type)) {                                                                 \
      case napi_float32_array: return reinterpret_cast<Expression<float> *>(object)->method(__VA_ARGS__); break;       \
      case napi_float64_array: return reinterpret_cast<Expression<double> *>(object)->method(__VA_ARGS__); break;      \
      default: return exprtk_invalid_argument;                                                                         \
    }                                                                                                                  \
  }
#endif

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
 * Get the number of threads available for evaluating expressions.
 * Set by the `EXPRTKJS_THREADS` environment variable.
 *
 * @readonly
 * @kind member
 * @name maxParallel
 * @static
 * @memberof Expression
 * @type {number}
 */

/**
 * Get/set the maximum allowed parallel instances for this Expression
 *
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
  if (newMax > maxParallel) {
    Napi::TypeError::New(
      env,
      "maximum instances is limited to the number of threads set by the environment variable EXPRTKJS_THREADS : " +
        std::to_string(maxParallel))
      .ThrowAsJavaScriptException();
    return;
  }
  maxParallel = newMax;
}

/**
 * Get the currently reached peak of simultaneously running instances for this Expression
 *
 * @readonly
 * @kind member
 * @name maxActive
 * @instance
 * @memberof Expression
 * @type {number}
 */
template <typename T> Napi::Value Expression<T>::GetMaxActive(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  return Napi::Number::New(env, maxActive);
}

/**
 * Get a string representation of this object
 *
 * @kind method
 * @name toString
 * @instance
 * @memberof Expression
 * @returns {string}
 */
template <typename T> Napi::Value Expression<T>::ToString(const Napi::CallbackInfo &info) {
  return GetExpression(info);
}

template <typename T> Napi::Function Expression<T>::GetClass(Napi::Env env) {
  const std::string className = std::string(NapiArrayType<T>::name) + "Expression";
  const Napi::Value maxParallel = Napi::Number::New(env, ExpressionMaxParallel);
  const Napi::Symbol toStringTag = Napi::Symbol::WellKnown(env, "toStringTag");
  const Napi::String type = Napi::String::New(env, NapiArrayType<T>::name);
  return Expression<T>::DefineClass(
    env,
    className.c_str(),
    {Expression<T>::InstanceAccessor("expression", &Expression<T>::GetExpression, nullptr, napi_enumerable),
     Expression<T>::InstanceAccessor("scalars", &Expression<T>::GetScalars, nullptr, napi_enumerable),
     Expression<T>::InstanceAccessor("vectors", &Expression<T>::GetVectors, nullptr, napi_enumerable),
     Expression<T>::InstanceValue("type", type, napi_enumerable),
     Expression<T>::StaticValue("type", type, napi_enumerable),
     Expression<T>::InstanceAccessor("_CAPI_", &Expression<T>::GetCAPI, nullptr, napi_default),
     Expression<T>::InstanceAccessor(
       "maxParallel", &Expression<T>::GetMaxParallel, &Expression<T>::SetMaxParallel, napi_enumerable),
     Expression<T>::InstanceAccessor("maxActive", &Expression<T>::GetMaxActive, nullptr, napi_enumerable),
     Expression<T>::StaticValue("maxParallel", maxParallel, napi_enumerable),
     Expression<T>::InstanceMethod(
       "toString", &Expression<T>::ToString, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
     Expression<T>::InstanceAccessor(toStringTag, &Expression<T>::ToString, nullptr, napi_default),
     ASYNCABLE_INSTANCE_METHOD(
       Expression<T>, eval, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
     ASYNCABLE_INSTANCE_METHOD(
       Expression<T>, map, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
     ASYNCABLE_INSTANCE_METHOD(
       Expression<T>, reduce, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
     ASYNCABLE_INSTANCE_METHOD(
       Expression<T>, cwise, static_cast<napi_property_attributes>(napi_writable | napi_configurable))});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  const char *exprtkjs_threads = std::getenv("EXPRTKJS_THREADS");
  if (exprtkjs_threads != nullptr) ExpressionMaxParallel = std::stoi(exprtkjs_threads);
  initAsyncWorkers(ExpressionMaxParallel);
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
} // namespace exprtk_js
