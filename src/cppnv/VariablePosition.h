#ifndef VARIABLEPOSITION_H
#define VARIABLEPOSITION_H
#include <string>

namespace cppnv {
struct variable_position {
  ~variable_position() {
    delete variable_str;
  }

  variable_position(int variable_start, int start_brace, int dollar_sign);
  int variable_start;
  int start_brace;
  int dollar_sign;
  int end_brace;
  int variable_end;
  std::string* variable_str;
  bool closed = false;
};
}
#endif // VARIABLEPOSITION_H
