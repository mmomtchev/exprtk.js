#pragma once

#include <cstdint>
#include <napi.h>

namespace exprtk_js {

template <typename T> struct NapiArrayType {};

#ifndef EXPRTK_DISABLE_INT_TYPES
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
#endif

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

template <> struct NapiArrayType<size_t> {
  static inline size_t CastFrom(const Napi::Value &value) {
    return static_cast<size_t>(value.As<Napi::Number>().Int64Value());
  }
};

} // namespace exprtk_js
