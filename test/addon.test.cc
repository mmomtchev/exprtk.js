#include <napi.h>

#include <exprtkjs.h>

Napi::Value TestEval(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value _CAPI_ = info[0].ToObject().Get("_CAPI_");
  if (_CAPI_.IsEmpty() || !_CAPI_.IsArrayBuffer()) {
    Napi::TypeError::New(env, "passed argument is not an Expression object").ThrowAsJavaScriptException();
    return env.Null();
  }

  exprtk_js::exprtk_expression *expr =
    reinterpret_cast<exprtk_js::exprtk_expression *>(_CAPI_.As<Napi::ArrayBuffer>().Data());
  if (expr->magic != EXPRTK_JS_CAPI_MAGIC) {
    Napi::TypeError::New(env, "bad Expression magic, corrupted object?").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->type != exprtk_js::napi_uint32_compatible) {
    Napi::TypeError::New(env, "Expression is not of Uint32 type").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->scalars_len != 2 || expr->vectors_len != 0) {
    Napi::TypeError::New(env, "Expression is not of the expected type (2 scalars / no vectors)")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  uint32_t ab[] = {12, 17};
  uint32_t r;

  expr->eval(expr, ab, nullptr, &r);

  return Napi::Number::New(env, r);
}

Napi::Value TestMap(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value _CAPI_ = info[0].ToObject().Get("_CAPI_");
  if (_CAPI_.IsEmpty() || !_CAPI_.IsArrayBuffer()) {
    Napi::TypeError::New(env, "passed argument is not an Expression object").ThrowAsJavaScriptException();
    return env.Null();
  }

  exprtk_js::exprtk_expression *expr =
    reinterpret_cast<exprtk_js::exprtk_expression *>(_CAPI_.As<Napi::ArrayBuffer>().Data());
  if (expr->magic != EXPRTK_JS_CAPI_MAGIC) {
    Napi::TypeError::New(env, "bad Expression magic, corrupted object?").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->type != exprtk_js::napi_uint32_compatible) {
    Napi::TypeError::New(env, "Expression is not of Uint32 type").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->scalars_len != 2 || expr->vectors_len != 0 || std::string(expr->scalars[0]) != "a") {
    Napi::TypeError::New(env, "Expression is not of the expected type (2 scalars, no vectors, first scalar is 'a')")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  uint32_t b[] = {20};
  uint32_t v[] = {10, 20, 30, 40, 50, 60};

  Napi::TypedArray r = Napi::Uint32Array::New(env, 6);

  expr->map(expr, "a", 6, &v, b, nullptr, r.ArrayBuffer().Data());

  return r;
}

Napi::Value TestReduce(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value _CAPI_ = info[0].ToObject().Get("_CAPI_");
  if (_CAPI_.IsEmpty() || !_CAPI_.IsArrayBuffer()) {
    Napi::TypeError::New(env, "passed argument is not an Expression object").ThrowAsJavaScriptException();
    return env.Null();
  }

  exprtk_js::exprtk_expression *expr =
    reinterpret_cast<exprtk_js::exprtk_expression *>(_CAPI_.As<Napi::ArrayBuffer>().Data());
  if (expr->magic != EXPRTK_JS_CAPI_MAGIC) {
    Napi::TypeError::New(env, "bad Expression magic, corrupted object?").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->type != exprtk_js::napi_uint32_compatible) {
    Napi::TypeError::New(env, "Expression is not of Uint32 type").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->scalars_len != 2 || expr->vectors_len != 0 || std::string(expr->scalars[0]) != "a") {
    Napi::TypeError::New(env, "Expression is not of the expected type (2 scalars, no vectors, first scalar is 'a')")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  uint32_t v[] = {10, 20, 30, 40, 50, 60};
  uint32_t r;

  expr->reduce(expr, "a", 6, v, "b", nullptr, nullptr, &r);

  return Napi::Number::New(env, r);
}

Napi::Value TestCwise(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "expression is mandatory").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Value _CAPI_ = info[0].ToObject().Get("_CAPI_");
  if (_CAPI_.IsEmpty() || !_CAPI_.IsArrayBuffer()) {
    Napi::TypeError::New(env, "passed argument is not an Expression object").ThrowAsJavaScriptException();
    return env.Null();
  }

  exprtk_js::exprtk_expression *expr =
    reinterpret_cast<exprtk_js::exprtk_expression *>(_CAPI_.As<Napi::ArrayBuffer>().Data());
  if (expr->magic != EXPRTK_JS_CAPI_MAGIC) {
    Napi::TypeError::New(env, "bad Expression magic, corrupted object?").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->type != exprtk_js::napi_float32_compatible) {
    Napi::TypeError::New(env, "Expression is not of Uint32 type").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (expr->scalars_len != 2 || expr->vectors_len != 0 || std::string(expr->scalars[0]) != "a") {
    Napi::TypeError::New(env, "Expression is not of the expected type (2 scalars, no vectors, first scalar is 'a')")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  uint8_t v1[] = {10, 20, 30, 40, 50};
  double v2[] = {1.0, 2.0, 3.0, 4.0, 5.0};

  static const exprtk_js::exprtk_capi_cwise_arg args[] = {
    {"a", exprtk_js::napi_uint8_compatible, 5, reinterpret_cast<void *>(v1)},
    {"b", exprtk_js::napi_float64_compatible, 5, reinterpret_cast<void *>(v2)}};

  Napi::TypedArray r = Napi::Float64Array::New(env, 5);
  static exprtk_js::exprtk_capi_cwise_arg result = {
    "a", exprtk_js::napi_float64_compatible, 5, reinterpret_cast<void *>(r.ArrayBuffer().Data())};

  expr->cwise(expr, 2, args, &result);

  return r;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "testEval"), Napi::Function::New(env, TestEval));
  exports.Set(Napi::String::New(env, "testMap"), Napi::Function::New(env, TestMap));
  exports.Set(Napi::String::New(env, "testReduce"), Napi::Function::New(env, TestReduce));
  exports.Set(Napi::String::New(env, "testCwise"), Napi::Function::New(env, TestCwise));
  return exports;
}

NODE_API_MODULE(exprtkjs_test, Init)
