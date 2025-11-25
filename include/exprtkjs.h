#ifndef EXPRTKJS_H
#define EXPRTKJS_H

#include <cstdint>

#ifdef __cplusplus
namespace exprtk_js {
extern "C" {
#endif

struct exprtk_expression;

typedef enum { exprtk_ok = 0, exprtk_invalid_argument } exprtk_result;

typedef enum {
  napi_int8_compatible,
  napi_uint8_compatible,
  napi_uint8_clamped_compatible,
  napi_int16_compatible,
  napi_uint16_compatible,
  napi_int32_compatible,
  napi_uint32_compatible,
  napi_float32_compatible,
  napi_float64_compatible,
} napi_compatible_type;

struct exprtk_capi_vector {
  const char *name;
  size_t elements;
};

struct exprtk_capi_cwise_arg {
  const char *name;
  napi_compatible_type type;
  size_t elements;
  void *data;
};

typedef exprtk_result
exprtkjs_capi_eval(exprtk_expression *expression, const void *scalars, void **vectors, void *result);
typedef exprtk_result exprtkjs_capi_map(
  exprtk_expression *expression,
  const char *iterator_name,
  const size_t iterator_len,
  const void *iterator_vector,
  const void *scalars,
  void **vectors,
  void *result);

typedef exprtk_result exprtkjs_capi_reduce(
  exprtk_expression *expression,
  const char *iterator_name,
  const size_t iterator_len,
  const void *iterator_vector,
  const char *accumulator,
  const void *scalars,
  void **vectors,
  void *result);

typedef exprtk_result exprtkjs_capi_cwise(
  exprtk_expression *expression, const size_t n_args, const exprtk_capi_cwise_arg *args, exprtk_capi_cwise_arg *result);

#define EXPRTK_JS_CAPI_MAGIC 0xC0DEDF0F00D

struct exprtk_expression {
  uint64_t magic;
  void *_descriptor_;

  const char *expression;
  napi_compatible_type type;

  size_t scalars_len;
  size_t vectors_len;

  const char **scalars;
  exprtk_capi_vector *vectors;

  exprtkjs_capi_eval *eval;
  exprtkjs_capi_map *map;
  exprtkjs_capi_reduce *reduce;
  exprtkjs_capi_cwise *cwise;
};

#ifdef __cplusplus
} // extern "C"
} // namespace exprtk_js
#endif

#endif
