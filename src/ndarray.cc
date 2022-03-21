#include <memory>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <array>
#include <napi.h>

#include "types.h"
#include "ndarray.h"

namespace exprtk_js {

// Create a raw array from a V8 array
template <typename T> static std::shared_ptr<T[]> NapiToRawArray(Napi::Array array) {
  std::shared_ptr<T[]> r = std::shared_ptr<T[]>(new T[array.Length()]);
  for (size_t i = 0; i < array.Length(); i++) r[i] = NapiArrayType<T>::CastFrom(array.Get(i));

  return r;
}

Napi::TypedArray StridedArrayBuffer(Napi::Object ndarray) {
  Napi::Value v;

  v = ndarray.Get("data");
  if (v.IsTypedArray()) return v.As<Napi::TypedArray>();

  v = ndarray.Get("_buffer");
  if (v.IsTypedArray()) return v.As<Napi::TypedArray>();

  return Napi::Value().As<Napi::TypedArray>();
}

Napi::Array StridedArrayShape(Napi::Object ndarray) {
  Napi::Value v;

  v = ndarray.Get("shape");
  if (v.IsArray()) return v.As<Napi::Array>();

  v = ndarray.Get("_shape");
  if (v.IsArray()) return v.As<Napi::Array>();

  return Napi::Value().As<Napi::Array>();
}

Napi::Array StridedArrayStride(Napi::Object ndarray) {
  Napi::Value v;

  v = ndarray.Get("stride");
  if (v.IsArray()) return v.As<Napi::Array>();

  v = ndarray.Get("strides");
  if (v.IsArray()) return v.As<Napi::Array>();

  v = ndarray.Get("_strides");
  if (v.IsArray()) return v.As<Napi::Array>();

  return Napi::Value().As<Napi::Array>();
}

size_t StridedArrayOffset(Napi::Object ndarray) {
  Napi::Value v;

  v = ndarray.Get("offset");
  if (v.IsNumber()) return static_cast<size_t>(v.ToNumber().Int64Value());

  v = ndarray.Get("_offset");
  if (v.IsNumber()) return static_cast<size_t>(v.ToNumber().Int64Value());

  return 0;
}

// Validate that the passed V8 object is a valid strided array and extract its dimensions data
bool ImportStridedArray(
  Napi::Value v, size_t &dims, int64_t &offset, std::shared_ptr<size_t[]> &shape, std::shared_ptr<int32_t[]> &stride) {
  auto env = v.Env();

  if (!v.IsObject()) return false;

  Napi::Object o = v.ToObject();

  Napi::Array v8shape = StridedArrayShape(o);
  Napi::Array v8stride = StridedArrayStride(o);
  Napi::TypedArray v8data = StridedArrayBuffer(o);
  offset = static_cast<size_t>(StridedArrayOffset(o));
  if (v8shape.IsEmpty() || v8stride.IsEmpty() || v8data.IsEmpty()) return false;

  if (v8shape.Length() != v8stride.Length())
    throw Napi::TypeError::New(env, "invalid strided array, shape.length != stride.length");

  shape = NapiToRawArray<size_t>(v8shape);
  stride = NapiToRawArray<int32_t>(v8stride);

  dims = v8shape.Length();
  int64_t lastElement = static_cast<int64_t>(offset);
  for (size_t i = 0; i < dims; i++) {
    if (shape[i] < 1) throw Napi::TypeError::New(env, "invalid strided array, non-positive shape");
    lastElement += static_cast<int64_t>((shape[i] - 1)) * static_cast<int64_t>(stride[i]);
  }
  if (lastElement < 0 || lastElement >= static_cast<int64_t>(v8data.ElementLength()))
    throw Napi::TypeError::New(env, "invalid strided array, ArrayBuffer overflow");

  return true;
}

// Transform a multi-dimensional strided index to a linear 1D offset
void GetLinearOffset(
  int64_t &offset,
  const std::shared_ptr<size_t[]> &index,
  const size_t dims,
  const std::shared_ptr<size_t[]> &shape,
  const std::shared_ptr<int32_t[]> &stride) {
  offset = 0;
  for (size_t d = 0; d < dims; d++) { offset += index[d] * stride[d]; }
}

// Transform a linear 1D offset to a multi-dimensional strided index
void GetStridedIndex(
  const int64_t offset,
  std::shared_ptr<size_t[]> &index,
  const size_t dims,
  const std::shared_ptr<size_t[]> &shape,
  const std::shared_ptr<int32_t[]> &stride) {

  int64_t linear = offset;

  std::vector<size_t> order(dims);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&stride](size_t a, size_t b) { return stride[a] > stride[b]; });

  for (auto const i : order) {
    auto const s = stride[i];
    int64_t k = linear / s;
    linear -= k * s;
    if (s < 0) {
      index[i] = shape[i] - 1 + k;
    } else {
      index[i] = k;
    }
  }
}

} // namespace exprtk_js
