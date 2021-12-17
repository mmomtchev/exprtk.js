#include "expression.h"

/**
 * @param {string} expression function
 * @param {string[]} variables
 * @param {{name: string, data: array}[]} [vectors]
 * @returns {Expression}
 *
 * @example
 * const mean = new Expression('(a+b)/2', ['a', 'b']);
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

            if (!value.IsTypedArray() || value.As<Napi::TypedArray>().TypedArrayType() != napi_float64_array) {
                Napi::TypeError::New(env, "vector data must be a Float64Array").ThrowAsJavaScriptException();
                return;
            }
            Napi::TypedArray data = value.As<Napi::TypedArray>();

            if (!symbolTable.add_vector(name, reinterpret_cast<double *>(data.ArrayBuffer().Data()), data.ElementLength())) {
                Napi::TypeError::New(env, name + " is not a valid vector name").ThrowAsJavaScriptException();
                return;
            }

            vectors.push_back(Napi::Persistent(value.As<Napi::Object>()));
        }
    }

    expression.register_symbol_table(symbolTable);

    expressionText = info[0].As<Napi::String>().Utf8Value();
    if (!parser.compile(expressionText, expression)) {
        std::string errorText = "failed compiling expression " + expressionText + "\n";
        for (std::size_t i = 0; i < parser.error_count(); i++) {
            exprtk::parser_error::type error = parser.get_error(i);
            errorText += exprtk::parser_error::to_str(error.mode) + " at " + std::to_string(error.token.position) + " : " + error.diagnostic + "\n";
        }
        Napi::Error::New(env, errorText).ThrowAsJavaScriptException();
    }
}

/**
 * Evaluate the expression
 *
 * @param {object} arguments function arguments
 * @returns {number}
 *
 * @example
 * const r = expr.eval({a: 2, b: 5});
 *
 * expr.evalAsync({a: 2, b: 5}, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(Expression::eval) {
    Napi::Env env = info.Env();

    if (info.Length() > 0) {
        if (!info[0].IsObject()) {
            Napi::TypeError::New(env, "arguments must be an object").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Object args = info[0].As<Napi::Object>();
        Napi::Array argNames = args.GetPropertyNames();
        for (std::size_t i = 0; i < argNames.Length(); i++) {
            const std::string name = argNames.Get(i).As<Napi::String>().Utf8Value();
            Napi::Value value = args.Get(name);
            if (!value.IsNumber()) {
                Napi::TypeError::New(env, name + " is not a number").ThrowAsJavaScriptException();
                return env.Null();
            }
            symbolTable.get_variable(name)->ref() = value.As<Napi::Number>().DoubleValue();
        }
    }

    ExprTkJob<double> job;
    job.main = [this]() { return expression.value(); };
    job.rval = [env](double r) { return Napi::Number::New(env, r); };
    return job.run(info, async, 1);
}

/**
 * Evaluate the expression
 *
 * @param {number} arg
 * @returns {number}
 *
 * @example
 * const r = expr.fn(2, 5);
 *
 * expr.fnAsync(2, 5, (e,r) => console.log(e, r));
 */
ASYNCABLE_DEFINE(Expression::fn) {
    Napi::Env env = info.Env();

    auto argLen = info.Length();
    if (async && argLen > 0 && info[argLen-1].IsFunction())
        argLen--;
    if (argLen != symbolTable.variable_count()) {
        Napi::TypeError::New(env, "The number of arguments must match the number of variables").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::vector<std::string> variableList;
    symbolTable.get_variable_list(variableList);

    for (std::size_t i = 0; i < argLen; i++) {
        if (!info[i].IsNumber()) {
            Napi::TypeError::New(env, "All arguments must be numbers").ThrowAsJavaScriptException();
            return env.Null();
        }
        symbolTable.get_variable(variableList[i])->ref() = info[i].As<Napi::Number>().DoubleValue();
    }

    ExprTkJob<double> job;
    job.main = [this]() { return expression.value(); };
    job.rval = [env](double r) { return Napi::Number::New(env, r); };
    return job.run(info, async, info.Length() - 1);
}

Napi::Function Expression::GetClass(Napi::Env env) {
    napi_property_attributes props = static_cast<napi_property_attributes>(napi_writable | napi_configurable);
    return DefineClass(env, "Expression",
                       {ASYNCABLE_INSTANCE_METHOD(Expression, eval, props), ASYNCABLE_INSTANCE_METHOD(Expression, fn, props)});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::String name = Napi::String::New(env, "Expression");
    exports.Set(name, Expression::GetClass(env));
    return exports;
}

NODE_API_MODULE(addon, Init)
