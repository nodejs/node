#include "EnvReader.h"


namespace cppnv {
env_reader::read_result env_reader::read_pair(EnvStream& file,
                                              const env_pair* pair) {
  const auto key_result = read_key(file, pair->key);

  switch (key_result) {
    case end_of_stream_key:
      return end_of_stream_key;
    case fail:
    case empty:
      return fail;
    case comment_encountered:
      //this means comment encountered in the key since you can encounter an empty value comment like "a=#"
      return comment_encountered;
    case end_of_stream_value:
      return success; //we know we hit = and endofstream
    case success:
      break;
  }
  if (!pair->key->has_own_buffer()) {
    const auto tmp_str = new std::string();
    tmp_str->resize(pair->key->key_index);

    tmp_str->replace(0,
                     pair->key->key_index,
                     *pair->key->key,
                     0,
                     pair->key->key_index);

    pair->key->set_own_buffer(tmp_str);
  } else {
    pair->key->clip_own_buffer(pair->key->key_index);
  }
  pair->value->value->clear();
  const auto value_result = read_value(file, pair->value);
  switch (value_result) {
    case end_of_stream_value:
      return end_of_stream_value; //implicitly a success "a="
    case comment_encountered:
    case success:
      if (!pair->value->has_own_buffer()) {
        const auto tmp_str = new std::string();
        tmp_str->resize(pair->value->value_index);
        tmp_str->replace(0,
                         pair->value->value_index,
                         *pair->value->value,
                         0,
                         pair->value->value_index);

        pair->value->set_own_buffer(tmp_str);
      } else {
        pair->value->clip_own_buffer(pair->value->value_index);
      }
      remove_unclosed_interpolation(pair->value);
      return success; //empty key is still success
    case empty:
      remove_unclosed_interpolation(pair->value);
      return empty;

    case fail:
      remove_unclosed_interpolation(pair->value);
      return fail;
    case end_of_stream_key:
      remove_unclosed_interpolation(pair->value);
      return end_of_stream_key;
  }
}

void env_reader::create_pair(std::string* const buffer, env_pair*& pair) {
  pair = new env_pair();
  pair->key = new env_key();
  pair->key->key = buffer;
  pair->value = new env_value();
  pair->value->value = buffer;
}

int env_reader::read_pairs(EnvStream& file, std::vector<env_pair*>* pairs) {
  int count = 0;
  auto buffer = std::string();
  buffer.resize(100);

  auto expect_more = true;
  while (expect_more) {
    buffer.clear();
    env_pair* pair;
    create_pair(&buffer, pair);
    const auto result = read_pair(file, pair);
    switch (result) {
      case end_of_stream_value:
        expect_more = false;
      case comment_encountered:
      case success:
        pairs->push_back(pair);
        count++;
        continue;
      case end_of_stream_key:
        expect_more = false;
      case fail:
      case empty:
        delete pair->key;
        delete pair->value;
        delete pair;
    }
  }

  return count;
}

void env_reader::delete_pair(const env_pair* pair) {
  delete pair->key;
  delete pair->value;
  delete pair;
}

void env_reader::delete_pairs(const std::vector<env_pair*>* pairs) {
  for (auto env_pair : *pairs) {
    delete_pair(env_pair);
  }
}

int env_reader::read_pairs(EnvStream& file,
                           std::map<std::string, env_pair*>* mapped_pairs) {
  int count = 0;
  auto buffer = std::string();

  auto expect_more = true;
  while (expect_more) {
    buffer.clear();
    env_pair* pair;
    create_pair(&buffer, pair);
    const auto result = read_pair(file, pair);
    switch (result) {
      case end_of_stream_value:
        expect_more = false;
      case comment_encountered:
      case success:

        mapped_pairs->insert_or_assign(*pair->key->key, pair);
        count++;
        continue;
      case end_of_stream_key:
        expect_more = false;
      case fail:
      case empty:
        delete pair->key;
        delete pair->value;
        delete pair;
    }
  }

  return count;
}

env_reader::read_result env_reader::position_of_dollar_last_sign(
    const env_value* value,
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
env_reader::read_result env_reader::read_key(EnvStream& file, env_key* key) {
  if (!file.good()) {
    return end_of_stream_key;
  }

  while (file.good()) {
    const auto key_char = file.get();
    if (key_char < 0) {
      break;
    }
    if (key_char == '#') {
      return comment_encountered;
    }
    switch (key_char) {
      case '=':
        if (!file.good()) {
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
    if (!file.good()) {
      return end_of_stream_key;
    }
  }
}

int env_reader::get_white_space_offset_left(const std::string* value,
                                            const variable_position*
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

int env_reader::get_white_space_offset_right(const std::string* value,
                                             const variable_position*
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

bool env_reader::process_possible_control_character(
    env_value* value,
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

void env_reader::walk_back_slashes(env_value* value) {
  const int total_backslash_pairs = value->back_slash_streak / 2;
  // how many \\ in a row

  if (total_backslash_pairs > 0) {
    for (int i = 0; i < total_backslash_pairs; i++) {
      add_to_buffer(value, '\\');
    }
    value->back_slash_streak -= total_backslash_pairs * 2;
  }
}

void env_reader::close_variable(env_value* value) {
  value->is_parsing_variable = false;
  variable_position* const interpolation = value->interpolations->at(
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

void env_reader::open_variable(env_value* value) {
  int position;
  const auto result = position_of_dollar_last_sign(value, &position);

  if (result == success) {
    value->is_parsing_variable = true;
    value->interpolations->push_back(
        new variable_position(value->value_index,
                              value->value_index - 1,
                              position));
  }
}

bool env_reader::walk_double_quotes(env_value* value) {
  switch (value->double_quote_streak) {
    case 1:
      if (value->triple_double_quoted) {
        return true;
      }
      if (value->double_quoted) {
        return false;
      }
      if (value->value_index == 0) {
        value->double_quoted = true;
      }
      return true;

    case 3:
      if (value->triple_double_quoted) {
        return false;
      }
      if (value->value_index == 0) {
        value->double_quoted = false;
        value->triple_double_quoted = true;
      }
      value->double_quote_streak = 0;
  }
  return true;
}

bool env_reader::walk_single_quotes(env_value* value) {
  if (value->quoted) {
    return true;
  }
  const auto quotes_at_start = value->value_index == 0;
  switch (value->single_quote_streak) {
    case 1:
      if (quotes_at_start) {
        value->quoted = true;
      }
      value->single_quote_streak = 0;
      return false;
    case 3:
      if (value->triple_quoted) {
        value->single_quote_streak = 0;
        return true;
      }
      if (quotes_at_start) {
        value->triple_quoted = true;
      }
      value->single_quote_streak = 0;
    default:
      if (value->single_quote_streak > 5) {
        value->value_index = value->value_index = value->single_quote_streak;
        value->single_quote_streak = 0;
        return true;
      }
  }
  return false;
}


void env_reader::add_to_buffer(env_value* value, const char key_char) {
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

bool env_reader::read_next_char(env_value* value, const char key_char) {
  if (!value->quoted && !value->triple_quoted && value->back_slash_streak > 0) {
    if (key_char != '\\') {
      walk_back_slashes(value);
      if (value->back_slash_streak == 1)
      // do we have an odd backslash out? ok, process control char
      {
        value->back_slash_streak = 0;
        if (process_possible_control_character(value, key_char)) {
          return true;
        }
        add_to_buffer(value, '\\');
      }
    }
  }
  switch (key_char) {
    case '\\':
      value->back_slash_streak++;

      return true;
    case '{':
      add_to_buffer(value, key_char);
      if (!value->is_parsing_variable) {
        //check to see if it's an escaped '{'
        if (!is_previous_char_an_escape(value)) {
          open_variable(value);
        }
      }
      return true;
    case '}':
      add_to_buffer(value, key_char);
      if (value->is_parsing_variable) {
        //check to see if it's an escaped '}'
        if (!is_previous_char_an_escape(value)) {
          close_variable(value);
        }
      }

      return true;
    case '\'':
      value->single_quote_streak++;
      if (walk_single_quotes(value)) {
        return true;
      }
      return true;
    case '"':
      value->double_quote_streak++;
      if (value->double_quote_streak > 0) {
        return walk_double_quotes(value);
      }
      return true;

    default:
      add_to_buffer(value, key_char);
      return true;
  }
}

// Used only when checking closed and open variables because the { }  have been added to the buffer
// it needs to check 2 values back.
bool env_reader::is_previous_char_an_escape(env_value* value) {
  return value->value_index > 1 && value->value->at(value->value_index - 2) ==
         '\\';
}

bool env_reader::clear_newline_or_comment(EnvStream& file,
                                          env_value* value,
                                          char key_char,
                                          env_reader::read_result& ret_value) {
  if (key_char == '\n' && !(value->triple_double_quoted || value->
                            triple_quoted)) {
    if (value->value_index > 0 && value->value->at(value->value_index - 1) ==
        '\r') {
      value->value_index--;
    }
    ret_value = success;
    return true;
  }
  if (key_char == '#') {
    if (!(value->quoted || value->double_quoted || value->triple_quoted || value
          ->triple_double_quoted)) {
      ret_value = comment_encountered;
      return true;
    }
    char tmp;
    do {
      tmp = file.get();
      if (tmp < 0) {
        break;
      }
      if (!file.good()) {
        break;
      }
    } while (tmp != '\n');
  }
  return false;
}

env_reader::read_result env_reader::read_value(EnvStream& file,
                                               env_value* value) {
  if (!file.good()) {
    return end_of_stream_value;
  }

  env_reader::read_result ret_val = success;
  while (file.good()) {
    char key_char = file.get();
    if (key_char < 0) {
      break;
    }
    if (clear_newline_or_comment(file, value, key_char, ret_val)) break;

    if (read_next_char(value, key_char) && file.good()) {
      continue;
    }
    if (value->triple_double_quoted || value->triple_quoted) {
      do {
        key_char = file.get();
        if (key_char < 0) {
          break;
        }
        if (!file.good()) {
          break;
        }
      } while (key_char != '\n');
    }
    break;
  }
  if (value->back_slash_streak > 0) {
    walk_back_slashes(value);
    if (value->back_slash_streak == 1) {
      process_possible_control_character(value, '\0');
    }
  }
  return success;
}

void env_reader::remove_unclosed_interpolation(env_value* value) {
  for (int i = value->interpolation_index - 1; i >= 0; i--) {
    const variable_position* interpolation = value->interpolations->at(i);
    if (interpolation->closed) {
      continue;
    }
    value->interpolations->erase(
        value->interpolations->begin() + value->interpolation_index);
    delete interpolation;
    value->interpolation_index--;
  }
}

env_reader::finalize_result env_reader::finalize_value(const env_pair* pair,
  std::map<std::string, env_pair*>* mapped_pairs) {
  if (pair->value->interpolation_index == 0) {
    pair->value->is_already_interpolated = true;
    pair->value->is_being_interpolated = false;
    return copied;
  }
  //const auto buffer = new std::string(*pair->value->value);
  const auto size = static_cast<int>(pair->value->interpolations->size());
  int buffer_size = pair->value->value_index;
  for (auto i = size - 1; i >= 0; i--) {
    const variable_position* interpolation = pair->value->interpolations->at(i);
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
  int index = pair->value->value_index - 1;
  int buffer_index = buffer_size;
  for (auto i = size - 1; i >= 0; i--) {
    const variable_position* interpolation = pair->value->interpolations->at(i);
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
      std::cout << *buffer << buffer_index << " " << count << std::endl;
    }

    auto variable_value_count = other_pair->value->value_index;
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

env_reader::finalize_result env_reader::finalize_value(
    const env_pair* pair,
    std::vector<env_pair*>* pairs) {
  if (pair->value->interpolation_index == 0) {
    pair->value->is_already_interpolated = true;
    pair->value->is_being_interpolated = false;
    return copied;
  }
  const auto buffer = new std::string(*pair->value->value);

  pair->value->set_own_buffer(buffer);

  const auto size = static_cast<int>(pair->value->interpolations->size());
  for (auto i = size - 1; i >= 0; i--) {
    const variable_position* interpolation = pair->value->interpolations->at(i);

    for (const env_pair* other_pair : *pairs) {
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
                      (interpolation->end_brace - interpolation->dollar_sign) +
                      1,
                      *other_pair->value->value);

      break;
    }
  }
  return interpolated;
}
}
