#ifndef ENVREADER_H
#define ENVREADER_H
#include <iostream>
#include <istream>
#include "EnvStream.h"
#include <map>
#include <vector>

#include "EnvPair.h"

namespace cppnv {
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
  static read_result position_of_dollar_last_sign(
      const EnvValue* value,
      int* position);
  static read_result read_key(EnvStream& file, env_key* key);
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
  static bool clear_newline_or_comment(EnvStream& file,
                                       EnvValue* value,
                                       char key_char,
                                       read_result& ret_value);
  static read_result read_value(EnvStream& file, EnvValue* value);
  static void remove_unclosed_interpolation(EnvValue* value);

public:
  static finalize_result finalize_value(const EnvPair* pair,
                                        std::map<std::string, EnvPair*>*
                                        mapped_pairs);
  static finalize_result finalize_value(const EnvPair* pair,
                                        std::vector<EnvPair*>* pairs);
  static read_result read_pair(EnvStream& file, const EnvPair* pair);
  static void create_pair(std::string* buffer, EnvPair*& pair);
  static int read_pairs(EnvStream& file, std::vector<EnvPair*>* pairs);
  static void delete_pair(const EnvPair* pair);
  static void delete_pairs(const std::vector<EnvPair*>* pairs);
  static int read_pairs(EnvStream& file,
                        std::map<std::string, EnvPair*>* mapped_pairs);
};
}
#endif // ENVREADER_H
