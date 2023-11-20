#ifndef SRC_CPPNV_ENVSTREAM_H_
#define SRC_CPPNV_ENVSTREAM_H_
#include "string"

namespace cppnv {
class EnvStream {
  size_t index_ = 0;
  std::string* data_ = nullptr;
  size_t length_;
  bool is_good_;

 public:
  explicit EnvStream(std::string* data);
  char get();
  [[nodiscard]] bool good() const;
  [[nodiscard]] bool eof() const;
};
}  // namespace cppnv

#endif  // SRC_CPPNV_ENVSTREAM_H_
