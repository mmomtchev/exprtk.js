#pragma once

#include <napi.h>

namespace exprtk_js {

Napi::TypedArray StridedArrayBuffer(Napi::Object ndarray);
Napi::Array StridedArrayShape(Napi::Object ndarray);
Napi::Array StridedArrayStride(Napi::Object ndarray);
size_t StridedArrayOffset(Napi::Object ndarray);

bool ImportStridedArray(
  Napi::Value v, size_t &dims, int64_t &offset, std::shared_ptr<size_t[]> &shape, std::shared_ptr<int32_t[]> &stride);

void GetStridedIndex(
  const int64_t offset,
  std::shared_ptr<size_t[]> &index,
  const size_t dims,
  const std::shared_ptr<size_t[]> &shape,
  const std::shared_ptr<int32_t[]> &stride);

void GetLinearOffset(
  int64_t &offset,
  const std::shared_ptr<size_t[]> &index,
  const size_t dims,
  const std::shared_ptr<size_t[]> &shape,
  const std::shared_ptr<int32_t[]> &stride);

// Get the next element in a strided array
inline void IncrementStridedIndex(
  std::shared_ptr<size_t[]> &index,
  uint8_t *start,
  uint8_t **ptr,
  const size_t elementSize,
  const size_t dims,
  const std::shared_ptr<size_t[]> &shape,
  const std::shared_ptr<int32_t[]> &stride) {

  for (int64_t d = dims - 1; d >= 0; d--) {
    index[d]++;
    if (index[d] < shape[d]) break;
    index[d] = 0;
  }

  *ptr = start;
  for (int64_t d = dims - 1; d >= 0; d--) { *ptr += index[d] * stride[d] * elementSize; }
}

template <typename T>
inline bool ArraysEqual(const std::shared_ptr<T[]> &a, const std::shared_ptr<T[]> &b, const size_t len) {
  for (size_t i = 0; i < len; i++)
    if (a[i] != b[i]) return false;
  return true;
}

inline size_t StridedLength(const std::shared_ptr<size_t[]> &shape, const size_t dims) {
  size_t length = 1;
  for (size_t i = 0; i < dims; i++) length *= shape[i];
  return length;
}

} // namespace exprtk_js