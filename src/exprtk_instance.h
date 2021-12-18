#pragma once

#include <exprtk.hpp>

extern template class exprtk::symbol_table<double>;
extern template class exprtk::expression<double>;

extern exprtk::parser<double> parser;
