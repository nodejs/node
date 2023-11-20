#include "EnvReader.h"


namespace cppnv {
EnvReader::read_result EnvReader::read_pair(EnvStream* file,
                                            const EnvPair* pair) {
  const read_result result = read_key(file, pair->key);
  if (result == fail || result == empty) {
    return fail;
  }
  if (result == comment_encountered) {
    return comment_encountered;
  }
  if (result == end_of_stream_key) {
    return end_of_stream_key;
  }

  if (result == end_of_stream_value) {
    return success;
  }
  //  trim right side of key
  while (pair->key->key_index > 0) {
    if (pair->key->key->at(pair->key->key_index - 1) != ' ') {
      break;
    }
    pair->key->key_index--;
  }
  if (!pair->key->has_own_buffer()) {
    const auto tmp_str = new std::string(pair->key->key_index, '\0');

    tmp_str->replace(0,
                     pair->key->key_index,
                     *pair->key->key,
                     0,
                     pair->key->key_index);

    pair->key->set_own_buffer(tmp_str);;
  } else {
    pair->key->clip_own_buffer(pair->key->key_index);
  }
  pair->value->value->clear();
  const read_result value_result = read_value(file, pair->value);
  if (value_result == end_of_stream_value) {
    return end_of_stream_value;
  }
  if (value_result == comment_encountered || value_result == success) {
    if (!pair->value->has_own_buffer()) {
      const auto tmp_str =
          new std::string(pair->value->value_index, '\0');

      tmp_str->replace(0,
                       pair->value->value_index,
                       *pair->value->value,
                       0,
                       pair->value->value_index);

      pair->value->
            set_own_buffer(tmp_str);
    } else {
      pair->value->clip_own_buffer(pair->value->value_index);
    }
    remove_unclosed_interpolation(pair->value);
    return success;
  }
  if (value_result == empty) {
    remove_unclosed_interpolation(pair->value);
    return empty;
  }
  if (value_result == end_of_stream_key) {
    remove_unclosed_interpolation(pair->value);
    return end_of_stream_key;
  }

  remove_unclosed_interpolation(pair->value);
  return fail;
}


int EnvReader::read_pairs(EnvStream* file, std::vector<EnvPair*>* pairs) {
  int count = 0;
  auto buffer = std::string();
  buffer.resize(100);

  auto expect_more = true;
  while (expect_more) {
    buffer.clear();
    EnvPair* pair = new EnvPair();
    pair->key = new EnvKey();
    pair->key->key = &buffer;
    pair->value = new EnvValue();
    pair->value->value = &buffer;
    const read_result result = read_pair(file, pair);
    if (result == end_of_stream_value) {
      expect_more = false;
      pairs->push_back(pair);
      count++;
      continue;
    }
    if (result == success) {
      pairs->push_back(pair);
      count++;
      continue;
    }
    if (result == end_of_stream_key || result == fail || result == empty) {
      if (result == end_of_stream_key) {
        expect_more = false;
      }
      delete pair->key;
      delete pair->value;
      delete pair;
    }
  }

  return count;
}

void EnvReader::delete_pair(const EnvPair* pair) {
  delete pair->key;
  delete pair->value;
  delete pair;
}

void EnvReader::delete_pairs(const std::vector<EnvPair*>* pairs) {
  for (const auto env_pair : *pairs) {
    delete_pair(env_pair);
  }
}

int EnvReader::read_pairs(EnvStream* file,
                          std::map<std::string, EnvPair*>* mapped_pairs) {
  int count = 0;
  auto buffer = std::string();

  auto expect_more = true;
  while (expect_more) {
    buffer.clear();
    EnvPair* pair = new EnvPair();
    pair->key = new EnvKey();
    pair->key->key = &buffer;
    pair->value = new EnvValue();
    pair->value->value = &buffer;

    const read_result read_pair_result = read_pair(file, pair);
    if (read_pair_result == end_of_stream_key || read_pair_result ==
        end_of_stream_value) {
      expect_more = false;
    }
    if (read_pair_result == end_of_stream_value || read_pair_result ==
        comment_encountered || read_pair_result == success) {
      mapped_pairs->insert_or_assign(*pair->key->key, pair);
      count++;
      continue;
    }
    delete pair->key;
    delete pair->value;
    delete pair;
  }

  return count;
}

void EnvReader::clear_garbage(EnvStream* file) {
  char key_char;
  do {
    key_char = file->get();
    if (key_char < 0) {
      break;
    }
    if (!file->good()) {
      break;
    }
  } while (key_char != '\n');
}

EnvReader::read_result EnvReader::position_of_dollar_last_sign(
    const EnvValue* value,
    int* position) {
  if (value->value_index < 1) {
    return empty;
  }
  auto tmp = value->value_index - 2;

  while (tmp >= 0) {
    if (value->value->at(tmp) == '$') {
      if (tmp > 0 && value->value->at(tmp - 1) == '\\') {
        return fail;
      }
      break;
    }
    if (value->value->at(tmp) == ' ') {
      tmp = tmp - 1;
      continue;
    }
    return fail;
  }
  *position = tmp;
  return success;
}

/**
 * \brief Assumes you've swept to new line before this and reads in a key.
 * \breif Anything is legal except newlines or =
 * \param file
 * \param key
 * \return
 */
EnvReader::read_result EnvReader::read_key(EnvStream* file, EnvKey* key) {
  if (!file->good()) {
    return end_of_stream_key;
  }

  while (file->good()) {
    const auto key_char = file->get();
    if (key_char < 0) {
      break;
    }
    if (key_char == '#') {
      return comment_encountered;
    }
    switch (key_char) {
      case ' ':
        if (key->key_index == 0) {
          continue;  // left trim keys
        }
        key->key->push_back(key_char);
        key->key_index++;  // I choose to support things like abc dc=ef
        break;
      case '=':
        if (!file->good()) {
          return end_of_stream_value;
        }
        return success;
      case '\r':
        continue;
      case '\n':
        return fail;
      default:
        key->key->push_back(key_char);
        key->key_index++;
    }
    if (!file->good()) {
      break;
    }
  }
  return end_of_stream_key;
}

int EnvReader::get_white_space_offset_left(const std::string* value,
                                           const VariablePosition*
                                           interpolation) {
  int tmp = interpolation->variable_start;
  int size = 0;
  while (tmp >= interpolation->start_brace) {
    if (value->at(tmp) != ' ') {
      break;
    }
    tmp = tmp - 1;
    size = size + 1;
  }
  return size;
}

int EnvReader::get_white_space_offset_right(const std::string* value,
                                            const VariablePosition*
                                            interpolation) {
  int tmp = interpolation->end_brace - 1;
  int count = 0;
  while (tmp >= interpolation->start_brace) {
    if (value->at(tmp) != ' ') {
      break;
    }
    count = count + 1;
    tmp = tmp - 1;
  }
  return count;
}

bool EnvReader::process_possible_control_character(
    EnvValue* value,
    const char key_char) {
  switch (key_char) {
    case '\0':
      return false;
    case 'v':

      add_to_buffer(value, '\v');
      return true;
    case 'a':

      add_to_buffer(value, '\a');
      return true;
    case 't':

      add_to_buffer(value, '\t');
      return true;
    case 'n':
      add_to_buffer(value, '\n');
      return true;
    case 'r':
      add_to_buffer(value, '\r');
      return true;
    case '"':
      add_to_buffer(value, '"');
      return true;
    case 'b':
      add_to_buffer(value, '\b');
      return true;
    case '\'':
      add_to_buffer(value, '\'');
      return true;
    case '\f':
      add_to_buffer(value, '\f');
      return true;
    default:
      return false;
  }
}

void EnvReader::walk_back_slashes(EnvValue* value) {
  const int total_backslash_pairs = value->back_slash_streak / 2;
  // how many \\ in a row

  if (total_backslash_pairs > 0) {
    for (int i = 0; i < total_backslash_pairs; i++) {
      add_to_buffer(value, '\\');
    }
    value->back_slash_streak -= total_backslash_pairs * 2;
  }
}

void EnvReader::close_variable(EnvValue* value) {
  value->is_parsing_variable = false;
  VariablePosition* const interpolation = value->interpolations->at(
      value->interpolation_index);
  interpolation->end_brace = value->value_index - 1;
  interpolation->variable_end = value->value_index - 2;
  const auto left_whitespace = get_white_space_offset_left(
      value->value,
      interpolation);
  if (left_whitespace > 0) {
    interpolation->variable_start =
        interpolation->variable_start + left_whitespace;
  }
  const auto right_whitespace = get_white_space_offset_right(
      value->value,
      interpolation);
  if (right_whitespace > 0) {
    interpolation->variable_end =
        interpolation->variable_end - right_whitespace;
  }
  const auto variable_len = (interpolation->variable_end - interpolation->
                             variable_start) + 1;
  interpolation->variable_str->resize(variable_len);
  interpolation->variable_str->replace(0,
                                       variable_len,
                                       *value->value,
                                       interpolation->variable_start,
                                       variable_len);
  interpolation->closed = true;
  value->interpolation_index++;
}

void EnvReader::open_variable(EnvValue* value) {
  int position;
  const auto result = position_of_dollar_last_sign(value, &position);

  if (result == success) {
    value->is_parsing_variable = true;
    value->interpolations->push_back(
        new VariablePosition(value->value_index,
                             value->value_index - 1,
                             position));
  }
}

bool EnvReader::walk_double_quotes(EnvValue* value) {
  // we have have some quotes at the start
  if (value->value_index == 0) {
    if (value->double_quote_streak == 1) {
      value->double_quote_streak = 0;
      value->double_quoted = true;
      return false;  // we have a double quote at the start
    }
    // we have a empty double quote value aka ''
    if (value->double_quote_streak == 2) {
      value->double_quote_streak = 0;
      value->double_quoted = true;
      return true;  // we have a  empty double quote at the start
    }
    if (value->double_quote_streak == 3) {
      value->double_quote_streak = 0;
      value->triple_double_quoted = true;
      return false;  // we have a triple double quote at the start
    }
    if (value->double_quote_streak > 5) {
      value->double_quote_streak = 0;
      value->triple_double_quoted = true;
      // basically we have """""" an empty heredoc with extra " at the end.
      // Ignore the trailing "
      return true;  // we have a triple quote at the start
    }
    if (value->double_quote_streak > 3) {
      value->triple_double_quoted = true;
      // we have """"...
      // add the diff to the buffer
      for (int i = 0; i < value->double_quote_streak - 3; i++) {
        add_to_buffer(value, '"');
      }
      value->double_quote_streak = 0;
      return false;  // we have a triple quote at the start
    }

    return false;  // we have garbage
  }

  // we're single quoted
  if (value->double_quoted) {
    // any amount of quotes sends
    value->double_quote_streak = 0;
    return true;
  }

  // We have a triple quote
  if (value->triple_double_quoted) {
    if (value->double_quote_streak == 3 || value->double_quote_streak > 3) {
      value->double_quote_streak = 0;
      return true;  // we have enough to close, truncate trailing single quotes
    }
    // we have not enough to close the heredoc.
    if (value->double_quote_streak < 3) {
      // add them to the buffer
      for (int i = 0; i < value->double_quote_streak; i++) {
        add_to_buffer(value, '"');
      }
      value->double_quote_streak = 0;
      return false;
    }
    return false;  // we have garbage
  }

  return false;
}

/**
 * \brief
 * \param value
 * \return True if end quotes detected and input should stop, false otherwise
 */
bool EnvReader::walk_single_quotes(EnvValue* value) {
  // if (value->quoted && value->single_quote_streak == 0) {
  //   return true;
  // }

  // we have have some quotes at the start
  if (value->value_index == 0) {
    if (value->single_quote_streak == 1) {
      value->single_quote_streak = 0;
      value->quoted = true;
      return false;  // we have a single quote at the start
    }
    // we have a empty single quote value aka ''
    if (value->single_quote_streak == 2) {
      value->single_quote_streak = 0;
      value->quoted = true;
      return true;  // we have a  empy quote at the start
    }
    if (value->single_quote_streak == 3) {
      value->single_quote_streak = 0;
      value->triple_quoted = true;
      return false;  // we have a triple quote at the start
    }
    if (value->single_quote_streak > 5) {
      value->single_quote_streak = 0;
      value->triple_quoted = true;
      // basically we have '''''' an empty heredoc with extra ' at the end.
      // Ignore the trailing '
      return true;  // we have a triple quote at the start
    }
    if (value->single_quote_streak > 3) {
      value->triple_quoted = true;
      // we have ''''...
      // add the diff to the buffer
      for (int i = 0; i < value->single_quote_streak - 3; i++) {
        add_to_buffer(value, '\'');
      }
      value->single_quote_streak = 0;
      return false;  // we have a triple quote at the start
    }

    return false;  // we have garbage
  }

  // we're single quoted
  if (value->quoted) {
    // any amount of quotes sends
    value->single_quote_streak = 0;
    return true;
  }

  // We have a triple quote
  if (value->triple_quoted) {
    if (value->single_quote_streak == 3 || value->single_quote_streak > 3) {
      value->single_quote_streak = 0;
      return true;  // we have enough to close, truncate trailing single quotes
    }
    // we have not enough to close the heredoc.
    if (value->single_quote_streak < 3) {
      // add them to the buffer
      for (int i = 0; i < value->single_quote_streak; i++) {
        add_to_buffer(value, '\'');
      }
      value->single_quote_streak = 0;
      return false;
    }
    return false;  // we have garbage
  }

  return false;
}


void EnvReader::add_to_buffer(EnvValue* value, const char key_char) {
  size_t size = value->value->size();
  if (static_cast<size_t>(value->value_index) >= size) {
    if (size == 0) {
      size = 100;
    }
    value->value->resize(size * 150 / 100);
  }
  (*value->value)[value->value_index] = key_char;
  value->value_index++;
}

bool EnvReader::read_next_char(EnvValue* value, const char key_char) {
  if (!value->quoted && !value->triple_quoted && value->back_slash_streak > 0) {
    if (key_char != '\\') {
      walk_back_slashes(value);
      if (value->back_slash_streak == 1) {
        // do we have an odd backslash out? ok, process control char
        value->back_slash_streak = 0;
        if (process_possible_control_character(value, key_char)) {
          return true;
        }
        add_to_buffer(value, '\\');
      }
    }
  }
  if (!value->triple_double_quoted && !value->double_quoted && value->
      single_quote_streak > 0) {
    if (key_char != '\'') {
      if (walk_single_quotes(value)) {
        return false;
      }
    }
  }
  if (!value->triple_quoted && !value->quoted && value->
      double_quote_streak > 0) {
    if (key_char != '"') {
      if (walk_double_quotes(value)) {
        return false;
      }
    }
  }
  // Check to see if the first character is a ' or ". If it is neither,
  // it is an implicit double quote.
  if (value->value_index == 0) {
    if (key_char == '`') {
      if (value->back_tick_quoted) {
        return false;
      }
      if (!value->quoted && !value->triple_quoted && !value->double_quoted &&
          !value->triple_double_quoted) {
        value->double_quoted = true;
        value->back_tick_quoted = true;
        return true;
      }
    }

    if (key_char == '#') {
      if (!value->quoted && !value->triple_quoted && !value->double_quoted &&
          !value->triple_double_quoted) {
        return false;
      }
    } else if (!(key_char == '"' || key_char == '\'')) {
      if (!value->quoted && !value->triple_quoted && !value->double_quoted
          && !value->triple_double_quoted) {
        value->double_quoted = true;
        value->implicit_double_quote = true;
      }
    }
    if (key_char == ' ' && value->implicit_double_quote) {
      return true;  // trim left strings on implicit quotes
    }
  }
  switch (key_char) {
    case '`':
      if (value->back_tick_quoted) {
        return false;
      }
      add_to_buffer(value, key_char);
      break;
    case '#':
      if (value->implicit_double_quote) {
        return false;
      }
      add_to_buffer(value, key_char);
      break;
    case '\n':
      if (!(value->triple_double_quoted || value->triple_quoted || (value->
              double_quoted && !value->implicit_double_quote))) {
        if (value->value_index > 0) {
          if (value->value->at(value->value_index - 1) == '\r') {
            value->value_index--;
          }
        }
        return false;
      }
      add_to_buffer(value, key_char);
      return true;
    case '\\':

      if (value->quoted || value->triple_quoted) {
        add_to_buffer(value, key_char);
        return true;
      }
      value->back_slash_streak++;

      return true;
    case '{':
      add_to_buffer(value, key_char);
      if (!value->quoted && !value->triple_quoted) {
        if (!value->is_parsing_variable) {
          // check to see if it's an escaped '{'
          if (!is_previous_char_an_escape(value)) {
            open_variable(value);
          }
        }
      }
      return true;
    case '}':
      add_to_buffer(value, key_char);
      if (value->is_parsing_variable) {
        // check to see if it's an escaped '}'
        if (!is_previous_char_an_escape(value)) {
          close_variable(value);
        }
      }

      return true;
    case '\'':
      if (!value->double_quoted && !value->triple_double_quoted) {
        value->single_quote_streak++;
      } else {
        add_to_buffer(value, key_char);
      }
      return true;

    case '"':
      if (!value->quoted && !value->triple_quoted && !value->back_tick_quoted &&
          !value->implicit_double_quote) {
        value->double_quote_streak++;
      } else {
        add_to_buffer(value, key_char);
      }
      return true;

    default:
      add_to_buffer(value, key_char);
  }
  return true;
}

// Used only when checking closed and open variables because the { }
// have been added to the buffer
// it needs to check 2 values back.
bool EnvReader::is_previous_char_an_escape(const EnvValue* value) {
  return value->value_index > 1
         && value->value->at(value->value_index - 2) ==
         '\\';
}


EnvReader::read_result EnvReader::read_value(EnvStream* file,
                                             EnvValue* value) {
  if (!file->good()) {
    return end_of_stream_value;
  }

  char key_char = 0;
  while (file->good()) {
    key_char = file->get();
    if (key_char < 0) {
      break;
    }
    // if (clear_newline_or_comment(file, value, key_char, ret_val))
    //   break;
    if (read_next_char(value, key_char) && file->good()) {
      continue;
    }
    // if (!(value->triple_double_quoted || value->triple_quoted)) {
    //   if (key_char == '\n') {
    //     break;
    //   }
    // }

    // if (value->triple_double_quoted || value->triple_quoted) {
    //   if (key_char != '\n') {
    //     clear_garbage(file);
    //   }
    // }
    break;
  }
  if (value->back_slash_streak > 0) {
    walk_back_slashes(value);
    if (value->back_slash_streak == 1) {
      process_possible_control_character(value, '\0');
    }
  }
  if (value->single_quote_streak > 0) {
    if (walk_single_quotes(value)) {
      if (key_char != '\n') {
        clear_garbage(file);
      }
    }
  }
  if (value->double_quote_streak > 0) {
    if (walk_double_quotes(value)) {
      if (key_char != '\n') {
        clear_garbage(file);
      }
    }
  }
  // trim right side of implicit double quote
  if (value->implicit_double_quote) {
    while (value->value_index > 0) {
      if (value->value->at(value->value_index - 1) != ' ') {
        break;
      }
      value->value_index--;
    }
  }
  return success;
}

void EnvReader::remove_unclosed_interpolation(EnvValue* value) {
  for (int i = value->interpolation_index - 1; i >= 0; i--) {
    const VariablePosition* interpolation = value->interpolations->at(i);
    if (interpolation->closed) {
      continue;
    }
    value->interpolations->erase(
        value->interpolations->begin() + value->interpolation_index);
    delete interpolation;
    value->interpolation_index--;
  }
}

EnvReader::finalize_result EnvReader::finalize_value(const EnvPair* pair,
  std::map<std::string, EnvPair*>* mapped_pairs) {
  if (pair->value->interpolation_index == 0) {
    pair->value->is_already_interpolated = true;
    pair->value->is_being_interpolated = false;
    return copied;
  }
  // const auto buffer = new std::string(*pair->value->value);
  const int size = pair->value->interpolations->size();
  int buffer_size = pair->value->value_index;
  for (auto i = size - 1; i >= 0; i--) {
    const VariablePosition* interpolation = pair->value->interpolations->at(i);
    const auto other_pair = (*mapped_pairs)[interpolation->variable_str->
      c_str()];
    const auto interpolation_length =
        (interpolation->end_brace - interpolation->dollar_sign) + 1;
    if (other_pair) {
      if (other_pair->value->is_being_interpolated) {
        return circular;
      }
      if (!other_pair->value->is_already_interpolated) {
        const auto walk_result = finalize_value(other_pair, mapped_pairs);
        if (walk_result == circular) {
          return circular;
        }
      }
      buffer_size += -(interpolation_length - other_pair->value->value_index);
    }
  }
  const auto buffer = new std::string();

  buffer->resize(buffer_size);
  const int index = pair->value->value_index - 1;
  int buffer_index = buffer_size;
  for (auto i = size - 1; i >= 0; i--) {
    const VariablePosition* interpolation = pair->value->interpolations->at(i);
    const auto other_pair = (*mapped_pairs)[*interpolation->variable_str];

    int count = (index - interpolation->end_brace);
    buffer_index = buffer_index - count;

    if (!other_pair) {
      buffer_index += count;
      count = index - interpolation->dollar_sign;
      buffer_index -= count;
      buffer->replace(buffer_index,
                      count,
                      *pair->value->value,
                      interpolation->dollar_sign,
                      count);
      continue;
    }
    if (count > 0) {
      buffer->replace(buffer_index,
                      count,
                      *pair->value->value,
                      interpolation->end_brace,
                      count);
    }

    const int variable_value_count = other_pair->value->value_index;
    buffer_index -= variable_value_count;
    buffer->replace(buffer_index,
                    variable_value_count,
                    *other_pair->value->value);
  }
  // if (index != 0)
  // {
  //     int off = buffer_index - index;
  //
  //     buffer->replace(off, index, *pair->value->value, 0, index);
  // }
  pair->value->set_own_buffer(buffer);
  return interpolated;
}

EnvReader::finalize_result EnvReader::finalize_value(
    const EnvPair* pair,
    std::vector<EnvPair*>* pairs) {
  if (pair->value->interpolation_index == 0) {
    pair->value->is_already_interpolated = true;
    pair->value->is_being_interpolated = false;
    return copied;
  }
  pair->value->is_being_interpolated = true;
  const auto buffer = new std::string(*pair->value->value);

  pair->value->set_own_buffer(buffer);

  const auto size = static_cast<int>(pair->value->interpolations->size());
  for (auto i = size - 1; i >= 0; i--) {
    const VariablePosition* interpolation = pair->value->interpolations->at(i);

    for (const EnvPair* other_pair : *pairs) {
      const size_t variable_str_len =
      (static_cast<size_t>(interpolation->variable_end) - interpolation->
       variable_start) + 1;
      if (variable_str_len != other_pair->key->
                                          key->size()) {
        continue;
      }

      if (0 != memcmp(other_pair->key->key->c_str(),
                      pair->value->value->c_str() + interpolation->
                      variable_start,
                      variable_str_len))
        continue;
      if (other_pair->value->is_being_interpolated) {
        return circular;
      }
      if (!other_pair->value->is_already_interpolated) {
        const auto walk_result = finalize_value(other_pair, pairs);
        if (walk_result == circular) {
          return circular;
        }
      }
      buffer->replace(interpolation->dollar_sign,
                      (interpolation->end_brace
                       - interpolation->dollar_sign) +
                      1,
                      *other_pair->value->value);

      break;
    }
  }
  pair->value->is_already_interpolated = true;
  pair->value->is_being_interpolated = false;
  return interpolated;
}
}  // namespace cppnv
