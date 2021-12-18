#include "expression.h"

using namespace exprtk_js;

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
 *  '(sumsq - (sum*sum) / x[]) / (n - 1);',
 *  [], {x: 1024})
 */
Expression::Expression(const Napi::CallbackInfo &info) : ObjectWrap(info) {
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
      double *dummy = (double *)&size;
      auto *v = new exprtk::vector_view<double>(dummy, size);
      vectorViews[name] = v;

      if (!symbolTable.add_vector(name, *v)) {
        Napi::TypeError::New(env, name + " is not a valid vector name").ThrowAsJavaScriptException();
        return;
      }
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

Expression::~Expression() {
  for (auto const &v : vectorViews) {
    v.second->rebase((double *)nullptr);
    delete v.second;
  }
  vectorViews.clear();
}

/**
 * Evaluate the expression
 *
 * @param {object} arguments function arguments
 * @returns {number}
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
ASYNCABLE_DEFINE(Expression::eval) {
  Napi::Env env = info.Env();

  ExprTkJob<double> job(asyncLock);

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
    return expression.value();
  };
  job.rval = [env](double r) { return Napi::Number::New(env, r); };
  return job.run(info, async, info.Length() - 1);
}

/**
 * Evaluate the expression for every element of a TypedArray
 * 
 * Evaluation and traversal happens entirely in C++ so this will be much
 * faster than calling TypedArray.prototype.map(expr.eval)
 *
 * @param {number} arg
 * @returns {number}
 *
 * @example
 * // r will be a TypedArray of the same type
 * const r = expr.map(array, exprArg1, exprArg2);
 *
 * expr.mapAsync(array, exprArg1, exprArg2, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(Expression::map) {
  Napi::Env env = info.Env();

  return env.Null();
}

Napi::Function Expression::GetClass(Napi::Env env) {
  napi_property_attributes props = static_cast<napi_property_attributes>(napi_writable | napi_configurable);
  return DefineClass(
    env,
    "Expression",
    {ASYNCABLE_INSTANCE_METHOD(Expression, eval, props)});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  Napi::String name = Napi::String::New(env, "Expression");
  exports.Set(name, Expression::GetClass(env));
  return exports;
}

NODE_API_MODULE(addon, Init)
