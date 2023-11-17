#include "VariablePosition.h"
namespace cppnv {
VariablePosition::VariablePosition(const int variable_start, const int start_brace, const int dollar_sign):
    variable_start(
        variable_start), start_brace(start_brace), dollar_sign(dollar_sign), end_brace(0), variable_end(0)
{
  variable_str = new std::string();
}
}
