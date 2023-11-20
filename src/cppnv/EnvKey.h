#ifndef ENVKEY_H
#define ENVKEY_H
#include <string>

namespace cppnv {
class EnvKey {
 public:
  std::string* key;  // this will be the temp buffer they all share
  std::string* own_buffer;
  // if this is set, then it has it's own buffer which you can get from key
  /**
   * \brief The current index in the buffer key
   */
  int key_index = 0;

  EnvKey()
    : key(nullptr) {
    own_buffer = nullptr;
  }

  void clip_own_buffer(int length) const {
    own_buffer->resize(length);
  }

  [[nodiscard]] bool has_own_buffer() const {
    return own_buffer != nullptr;
  }


  void set_own_buffer(std::string* buff) {
    delete own_buffer;
    own_buffer = buff;
    key = buff;
  }

  ~EnvKey() {
    delete own_buffer;
  }
};
}  // namespace cppnv
#endif  // ENVKEY_H
