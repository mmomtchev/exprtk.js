#pragma once

#include <napi.h>
#include "exprtk_instance.h"
#include "async.h"

class Expression : public Napi::ObjectWrap<Expression> {
  public:
    Expression(const Napi::CallbackInfo &);
  
    ASYNCABLE_DECLARE(eval);
    ASYNCABLE_DECLARE(fn);

    static Napi::Function GetClass(Napi::Env);

  private:
    std::string expressionText;
    exprtk::symbol_table<double> symbolTable;
    exprtk::expression<double> expression;
    std::vector<Napi::ObjectReference> vectors;
};
