#ifndef ENVSTREAM_H
#define ENVSTREAM_H
#include "string"

namespace cppnv {
class EnvStream {
private:
  size_t index_ = 0;
  std::string* data_ = nullptr;
  size_t length_;
  bool is_good;

public:
  EnvStream(std::string* data);
  char get();
  bool good();
  bool eof();
};
}

#endif
