#include <exprtk.hpp>

// Explicit instantiation avoids rebuilding the 40+ thousands lines template
// at each incremental rebuild

template class exprtk::symbol_table<double>;
template class exprtk::expression<double>;

exprtk::parser<double> parser;
