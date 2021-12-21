#include "expression.h"

#include <memory>

using namespace exprtk_js;

template <typename T> exprtk::parser<T> Expression<T>::parser;

/**
 * @param {string} expression function
 * @param {string[]} variables An array containing all the scalar variables' names
 * @param {Record<string,number>[]} [vectors] An object containing all the vector variables' names and their sizes
 * @returns {Expression}
 *
 * @example
 * // arithmetic mean of 2 variables
 * const mean = new Expression('(a+b)/2', ['a', 'b']);
 * 
 * // stddev of an array of 1024 elements
 * const stddev = new Expression(
 *  'var sum := 0; var sumsq := 0; ' + 
 *  'for (var i := 0; i < x[]; i += 1) { sum += x[i]; sumsq += x[i] * x[i] }; ' +
 *  '(sumsq - (sum*sum) / x[]) / (x[] - 1);',
 *  [], {x: 1024})
 */
template <typename T>
Expression<T>::Expression(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Expression<T>>::ObjectWrap(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 2) {
    Napi::TypeError::New(env, "arguments is mandatory").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "expresion must be a string").ThrowAsJavaScriptException();
    return;
  }

  if (!info[1].IsArray()) {
    Napi::TypeError::New(env, "arguments must be an array").ThrowAsJavaScriptException();
    return;
  }

  Napi::Array args = info[1].As<Napi::Array>();
  for (std::size_t i = 0; i < args.Length(); i++) {
    const std::string name = args.Get(i).As<Napi::String>().Utf8Value();
    if (!symbolTable.create_variable(name)) {
      Napi::TypeError::New(env, name + " is not a valid variable name").ThrowAsJavaScriptException();
      return;
    }
    variableNames.push_back(name);
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
      vectorViews[name] = v;

      if (!symbolTable.add_vector(name, *v)) {
        Napi::TypeError::New(env, name + " is not a valid vector name").ThrowAsJavaScriptException();
        return;
      }

      variableNames.push_back(name);
    }
  }

  expression.register_symbol_table(symbolTable);

  expressionText = info[0].As<Napi::String>().Utf8Value();
  if (!parser.compile(expressionText, expression)) {
    std::string errorText = "failed compiling expression " + expressionText + "\n";
    for (std::size_t i = 0; i < parser.error_count(); i++) {
      exprtk::parser_error::type error = parser.get_error(i);
      errorText += exprtk::parser_error::to_str(error.mode) + " at " + std::to_string(error.token.position) + " : " +
        error.diagnostic + "\n";
    }
    Napi::Error::New(env, errorText).ThrowAsJavaScriptException();
  }
}

template <typename T> Expression<T>::~Expression() {
  for (auto const &v : vectorViews) {
    v.second->rebase((T *)nullptr);
    delete v.second;
  }
  vectorViews.clear();
}

/**
 * Evaluate the expression
 *
 * @param {object} arguments function arguments
 * @returns {number|TypedArray}
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

  ExprTkJob<T> job(asyncLock);

  std::vector<std::function<void()>> importers;

  if (info.Length() > 0 && info[0].IsObject() && !info[0].IsTypedArray()) {
    importFromObject(env, job, info[0], importers);
  }

  if (info.Length() > 0 && (info[0].IsNumber() || info[0].IsTypedArray())) {
    size_t last = info.Length();
    if (async && last > 0 && info[last - 1].IsFunction()) last--;
    importFromArgumentsArray(env, job, info, 0, last, importers);
  }

  if (symbolTable.variable_count() + symbolTable.vector_count() != importers.size()) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  job.main = [this, importers]() {
    for (auto const &f : importers) f();
    T r = expression.value();
    if (expression.results().count()) { throw "explicit return values are not supported"; }
    return r;
  };
  job.rval = [env](T r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

/**
 * Evaluate the expression for every element of a TypedArray
 * 
 * Evaluation and traversal happens entirely in C++ so this will be much
 * faster than calling array.map(expr.eval)
 *
 * @param {number} arg
 * @returns {number}
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

  ExprTkJob<T> job(asyncLock);

  std::vector<std::function<void()>> importers;

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
  auto iterator = symbolTable.get_variable(iteratorName);
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

  if (symbolTable.variable_count() + symbolTable.vector_count() != importers.size() + 1) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::TypedArray result = NapiArrayType<T>::New(env, len);
  T *output = reinterpret_cast<T *>(result.ArrayBuffer().Data());

  // this should have been an unique_ptr
  // but std::function is not compatible with move semantics
  std::shared_ptr<Napi::ObjectReference> persistent =
    std::make_shared<Napi::ObjectReference>(Napi::ObjectReference::New(result));

  job.main = [this, importers, iterator, input, output, len]() {
    for (auto const &f : importers) f();

    T *it_ptr = &iterator->ref();
    T *in_ptr = input;
    T *out_ptr = output;
    auto const in_end = in_ptr + len;
    for (; in_ptr < in_end; in_ptr++, out_ptr++) {
      *it_ptr = *in_ptr;
      *out_ptr = expression.value();
    }
    return 0;
  };
  job.rval = [env, persistent](T r) { return persistent->Value(); };
  return job.run(info, async, info.Length() - 1);
}

/**
 * Evaluate the expression for every element of a TypedArray
 * passing a scalar accumulator to every evaluation
 * 
 * Evaluation and traversal happens entirely in C++ so this will be much
 * faster than calling array.reduce(expr.eval)
 *
 * @param {number} arg
 * @returns {number}
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

  ExprTkJob<T> job(asyncLock);

  std::vector<std::function<void()>> importers;

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
  auto iterator = symbolTable.get_variable(iteratorName);
  if (iterator == nullptr) {
    Napi::TypeError::New(env, iteratorName + " is not a declared scalar variable").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 3 || !info[2].IsString()) {
    Napi::TypeError::New(env, "third argument must be the accumulator variable name").ThrowAsJavaScriptException();
    return env.Null();
  }
  const std::string accuName = info[2].As<Napi::String>().Utf8Value();
  auto accu = symbolTable.get_variable(accuName);
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

  if (symbolTable.variable_count() + symbolTable.vector_count() != importers.size() + 2) {
    Napi::TypeError::New(env, "wrong number of input arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  job.main = [this, importers, iterator, accu, accuInit, input, len]() {
    for (auto const &f : importers) f();
    accu->ref() = accuInit;
    T *it_ptr = &iterator->ref();
    T *accu_ptr = &accu->ref();
    T *input_end = input + len;
    for (T *i = input; i < input_end; i++) {
      *it_ptr = *i;
      *accu_ptr = expression.value();
    }
    return accu->value();
  };
  job.rval = [env](T r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

/**
 * Generic vector operation with implicit traversal
 * 
 * Supports automatic type conversions, multiple inputs and writing into a pre-existing array
 *
 * @param {Record<string, number | TypedArray>} arguments
 * @param {TypedArray} [output] output array, a new array of the expression type is automatically allocated if none is specified
 * @returns {TypedArray}
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
 * const phi = new Float32Array([0, 0.2, 0.5, 0.9, 0.5]);
 * const P = new Uint16Array([1013, 1013, 1013, 1013, 995]);
 * const T = new Uint16Array([25, 25, 25, 25, 25]);
 * 
 * const density = new Float64Expression(
 *   'Pv := ( phi * 6.1078 * pow(10, (7.5 * T / (T + 237.3))) ); ' +  // compute Pv and store it
 *   '( (P - Pv) * Md + Pv * Mv ) / ( R * (T + 273.13) )',           // return expression
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

  ExprTkJob<T> job(asyncLock);

  typedef std::function<T(uint8_t *)> NapiFromCaster_t;
  typedef std::function<void(uint8_t *, T)> NapiToCaster_t;
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
  static const NapiFromCaster_t NapiFromCasters[] = {
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

  static const NapiToCaster_t NapiToCasters[] = {
    [](uint8_t *dst, T value) { *(reinterpret_cast<int8_t *>(dst)) = static_cast<int8_t>(value); },
    [](uint8_t *dst, T value) { *dst = value; },
    [](uint8_t *dst, T value) { throw "unsupported type"; },
    [](uint8_t *dst, T value) { *(reinterpret_cast<int16_t *>(dst)) = static_cast<int16_t>(value); },
    [](uint8_t *dst, T value) { *(reinterpret_cast<uint16_t *>(dst)) = static_cast<uint16_t>(value); },
    [](uint8_t *dst, T value) { *(reinterpret_cast<int32_t *>(dst)) = static_cast<int32_t>(value); },
    [](uint8_t *dst, T value) { *(reinterpret_cast<uint32_t *>(dst)) = static_cast<uint32_t>(value); },
    [](uint8_t *dst, T value) { *(reinterpret_cast<float *>(dst)) = static_cast<float>(value); },
    [](uint8_t *dst, T value) { *(reinterpret_cast<double *>(dst)) = static_cast<double>(value); },
    [](uint8_t *dst, T value) { throw "unsupported type"; },
    [](uint8_t *dst, T value) { throw "unsupported type"; }};

  if (info.Length() < 1 || !info[0].IsObject()) {

    Napi::TypeError::New(env, "first argument must be a an object containing the input values")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (symbolTable.vector_count() > 0) {
    Napi::TypeError::New(env, "cwise()/cwiseAsync() are not compatible with vector arguments")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > 1 && !info[1].IsTypedArray() && (!async || !info[1].IsFunction())) {
    Napi::TypeError::New(env, "second argument must be a TypedArray or undefined").ThrowAsJavaScriptException();
    return env.Null();
  }

  bool typeConversionRequired = false;

  struct symbolDesc {
    std::string name;
    napi_typedarray_type type;
    uint8_t *data;
    uint8_t storage[8];
    size_t elementSize;
    T *exprtk_var;
    NapiFromCaster_t fromCaster;
  };

  size_t len = 0;
  Napi::Object args = info[0].As<Napi::Object>();
  Napi::Array argNames = args.GetPropertyNames();
  std::vector<symbolDesc> scalars, vectors;
  for (std::size_t i = 0; i < argNames.Length(); i++) {
    const std::string name = argNames.Get(i).As<Napi::String>().Utf8Value();
    Napi::Value value = args.Get(name);
    auto exprtk_ptr = symbolTable.get_variable(name);
    if (exprtk_ptr == nullptr) {
      Napi::TypeError::New(env, name + " is not a declared scalar variable").ThrowAsJavaScriptException();
      return env.Null();
    }
    symbolDesc current;
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
      current.fromCaster = NapiFromCasters[current.type];
      if (current.type != NapiArrayType<T>::type) typeConversionRequired = true;
      vectors.push_back(current);
    } else {
      variableNames.push_back(name);
      Napi::TypeError::New(env, name + " is not a number or a TypedArray").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  if (symbolTable.variable_count() != scalars.size() + vectors.size()) {
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
  const NapiToCaster_t toCaster = NapiToCasters[outputType];
  if (outputType != NapiArrayType<T>::type) typeConversionRequired = true;

  std::shared_ptr<Napi::ObjectReference> persistent =
    std::make_shared<Napi::ObjectReference>(Napi::ObjectReference::New(result));

  if (typeConversionRequired) {
    job.main = [this, scalars, vectors, output, elementSize, len, toCaster]() mutable {
      for (auto const &v : scalars) { *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage)); }

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
    job.main = [this, scalars, vectors, output, len]() mutable {
      for (auto const &v : scalars) { *v.exprtk_var = *(reinterpret_cast<const T *>(v.storage)); }

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

  job.rval = [env, persistent](T r) { return persistent->Value(); };
  return job.run(info, async, info.Length() - 1);
}

template <typename T> Napi::Function Expression<T>::GetClass(Napi::Env env) {
  napi_property_attributes props = static_cast<napi_property_attributes>(napi_writable | napi_configurable);
  return Napi::ObjectWrap<Expression<T>>::DefineClass(
    env,
    NapiArrayType<T>::name,
    {ASYNCABLE_INSTANCE_METHOD(Expression, eval, props),
     ASYNCABLE_INSTANCE_METHOD(Expression, map, props),
     ASYNCABLE_INSTANCE_METHOD(Expression, reduce, props),
     ASYNCABLE_INSTANCE_METHOD(Expression, cwise, props)});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
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

NODE_API_MODULE(addon, Init)
