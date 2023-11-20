#ifndef SRC_CPPNV_VARIABLEPOSITION_H_
#define SRC_CPPNV_VARIABLEPOSITION_H_
#include <string>

namespace cppnv {
struct VariablePosition {
  ~VariablePosition() {
    delete variable_str;
  }

  VariablePosition(int variable_start, int start_brace, int dollar_sign);
  int variable_start;
  int start_brace;
  int dollar_sign;
  int end_brace;
  int variable_end;
  std::string* variable_str;
  bool closed = false;
};
}  // namespace cppnv
#endif  // SRC_CPPNV_VARIABLEPOSITION_H_
