#ifndef SRC_NODE_DOTENV_H_
#define SRC_NODE_DOTENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util-inl.h"

#include <map>
#include <string>
#include <cstring>
#include <vector>

namespace node {

class Dotenv {
 public:
  Dotenv() = default;
  Dotenv(const Dotenv& d) = default;
  Dotenv(Dotenv&& d) noexcept = default;
  Dotenv& operator=(Dotenv&& d) noexcept = default;
  Dotenv& operator=(const Dotenv& d) = default;
  ~Dotenv() = default;

  bool ParsePath(const std::string_view path);
  void AssignNodeOptionsIfAvailable(std::string* node_options);
  void SetEnvironment(Environment* env);

  static std::vector<std::string> GetPathFromArgs(
      const std::vector<std::string>& args);

 private:
  void ParseLine(const std::string_view line);
  std::map<std::string, std::string> store_;
};

}  // namespace node

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
struct EnvValue {
  std::string* value;
  bool is_parsing_variable = false;
  std::vector<VariablePosition*>* interpolations;
  int interpolation_index = 0;
  bool quoted = false;
  bool triple_quoted = false;
  bool double_quoted = false;
  bool triple_double_quoted = false;
  bool implicit_double_quote = false;
  bool back_tick_quoted = false;
  int value_index = 0;
  bool is_already_interpolated = false;
  bool is_being_interpolated = false;
  bool did_over_flow = false;
  int back_slash_streak = 0;
  int single_quote_streak = 0;
  int double_quote_streak = 0;
  std::string* own_buffer;


  void clip_own_buffer(int length) const {
    own_buffer->resize(length);
  }

  bool has_own_buffer() const {
    return own_buffer != nullptr;
  }

  void set_own_buffer(std::string* buff) {
    delete own_buffer;
    own_buffer = buff;
    value = buff;
  }

  EnvValue(): value(nullptr), own_buffer(nullptr) {
    interpolations = new std::vector<VariablePosition*>();
  }

  ~EnvValue() {
    for (const auto interpolation : *interpolations) {
      delete interpolation;
    }
    delete own_buffer;
    delete interpolations;
  }
};
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
struct EnvPair {
  EnvKey* key;
  EnvValue* value;
};
class EnvReader {
 public:
  enum read_result {
    success,
    empty,
    fail,
    comment_encountered,
    end_of_stream_key,
    end_of_stream_value
  };

  enum finalize_result { interpolated, copied, circular };

 private:
  static void clear_garbage(EnvStream* file);
  static read_result position_of_dollar_last_sign(
      const EnvValue* value,
      int* position);
  static read_result read_key(EnvStream* file, EnvKey* key);
  static int get_white_space_offset_left(const std::string* value,
                                         const VariablePosition*
                                         interpolation);

  static int get_white_space_offset_right(const std::string* value,
                                          const VariablePosition*
                                          interpolation);
  static bool process_possible_control_character(
      EnvValue* value,
      const char key_char);
  static void walk_back_slashes(EnvValue* value);
  static void close_variable(EnvValue* value);
  static void open_variable(EnvValue* value);
  static bool walk_double_quotes(EnvValue* value);
  static bool walk_single_quotes(EnvValue* value);
  static void add_to_buffer(EnvValue* value, char key_char);
  static bool read_next_char(EnvValue* value, char key_char);
  static bool is_previous_char_an_escape(const EnvValue* value);

  static read_result read_value(EnvStream* file, EnvValue* value);
  static void remove_unclosed_interpolation(EnvValue* value);

 public:
  static finalize_result finalize_value(const EnvPair* pair,
                                        std::vector<EnvPair*>* pairs);
  static read_result read_pair(EnvStream* file, const EnvPair* pair);

  static int read_pairs(EnvStream* file, std::vector<EnvPair*>* pairs);
  static void delete_pair(const EnvPair* pair);
  static void delete_pairs(const std::vector<EnvPair*>* pairs);
};
}  // namespace cppnv
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DOTENV_H_
