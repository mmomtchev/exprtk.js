#pragma once

// This is a particularly dangerous form of cheating:
// When building the CPP files a simplified version of the template is used
// Then, when linking, the full enhanced version will be used
// Will this blow up in my face one day? We will see
// In the mean time, it allows for reasonable build times of the debug builds
#define exprtk_disable_enhanced_features

#include <exprtk.hpp>

extern template class exprtk::symbol_table<double>;
extern template class exprtk::expression<double>;

extern exprtk::parser<double> parser;
