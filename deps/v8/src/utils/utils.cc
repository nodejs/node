// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/utils.h"

#include <stdarg.h>
#include <sys/stat.h>

#include <vector>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

SimpleStringBuilder::SimpleStringBuilder(int size) {
  buffer_ = Vector<char>::New(size);
  position_ = 0;
}

void SimpleStringBuilder::AddString(const char* s) {
  size_t len = strlen(s);
  DCHECK_GE(kMaxInt, len);
  AddSubstring(s, static_cast<int>(len));
}

void SimpleStringBuilder::AddSubstring(const char* s, int n) {
  DCHECK(!is_finalized() && position_ + n <= buffer_.length());
  DCHECK_LE(n, strlen(s));
  MemCopy(&buffer_[position_], s, n * kCharSize);
  position_ += n;
}

void SimpleStringBuilder::AddPadding(char c, int count) {
  for (int i = 0; i < count; i++) {
    AddCharacter(c);
  }
}

void SimpleStringBuilder::AddDecimalInteger(int32_t value) {
  uint32_t number = static_cast<uint32_t>(value);
  if (value < 0) {
    AddCharacter('-');
    number = static_cast<uint32_t>(-value);
  }
  int digits = 1;
  for (uint32_t factor = 10; digits < 10; digits++, factor *= 10) {
    if (factor > number) break;
  }
  position_ += digits;
  for (int i = 1; i <= digits; i++) {
    buffer_[position_ - i] = '0' + static_cast<char>(number % 10);
    number /= 10;
  }
}

char* SimpleStringBuilder::Finalize() {
  DCHECK(!is_finalized() && position_ <= buffer_.length());
  // If there is no space for null termination, overwrite last character.
  if (position_ == buffer_.length()) {
    position_--;
    // Print ellipsis.
    for (int i = 3; i > 0 && position_ > i; --i) buffer_[position_ - i] = '.';
  }
  buffer_[position_] = '\0';
  // Make sure nobody managed to add a 0-character to the
  // buffer while building the string.
  DCHECK(strlen(buffer_.begin()) == static_cast<size_t>(position_));
  position_ = -1;
  DCHECK(is_finalized());
  return buffer_.begin();
}

std::ostream& operator<<(std::ostream& os, FeedbackSlot slot) {
  return os << "#" << slot.id_;
}

size_t hash_value(BailoutId id) {
  base::hash<int> h;
  return h(id.id_);
}

std::ostream& operator<<(std::ostream& os, BailoutId id) {
  return os << id.id_;
}

void PrintF(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  base::OS::VPrint(format, arguments);
  va_end(arguments);
}

void PrintF(FILE* out, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  base::OS::VFPrint(out, format, arguments);
  va_end(arguments);
}

void PrintPID(const char* format, ...) {
  base::OS::Print("[%d] ", base::OS::GetCurrentProcessId());
  va_list arguments;
  va_start(arguments, format);
  base::OS::VPrint(format, arguments);
  va_end(arguments);
}

void PrintIsolate(void* isolate, const char* format, ...) {
  base::OS::Print("[%d:%p] ", base::OS::GetCurrentProcessId(), isolate);
  va_list arguments;
  va_start(arguments, format);
  base::OS::VPrint(format, arguments);
  va_end(arguments);
}

int SNPrintF(Vector<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}

int VSNPrintF(Vector<char> str, const char* format, va_list args) {
  return base::OS::VSNPrintF(str.begin(), str.length(), format, args);
}

void StrNCpy(Vector<char> dest, const char* src, size_t n) {
  base::OS::StrNCpy(dest.begin(), dest.length(), src, n);
}

char* ReadLine(const char* prompt) {
  char* result = nullptr;
  char line_buf[256];
  size_t offset = 0;
  bool keep_going = true;
  fprintf(stdout, "%s", prompt);
  fflush(stdout);
  while (keep_going) {
    if (fgets(line_buf, sizeof(line_buf), stdin) == nullptr) {
      // fgets got an error. Just give up.
      if (result != nullptr) {
        DeleteArray(result);
      }
      return nullptr;
    }
    size_t len = strlen(line_buf);
    if (len > 1 && line_buf[len - 2] == '\\' && line_buf[len - 1] == '\n') {
      // When we read a line that ends with a "\" we remove the escape and
      // append the remainder.
      line_buf[len - 2] = '\n';
      line_buf[len - 1] = 0;
      len -= 1;
    } else if ((len > 0) && (line_buf[len - 1] == '\n')) {
      // Since we read a new line we are done reading the line. This
      // will exit the loop after copying this buffer into the result.
      keep_going = false;
    }
    if (result == nullptr) {
      // Allocate the initial result and make room for the terminating '\0'
      result = NewArray<char>(len + 1);
    } else {
      // Allocate a new result with enough room for the new addition.
      size_t new_len = offset + len + 1;
      char* new_result = NewArray<char>(new_len);
      // Copy the existing input into the new array and set the new
      // array as the result.
      MemCopy(new_result, result, offset * kCharSize);
      DeleteArray(result);
      result = new_result;
    }
    // Copy the newly read line into the result.
    MemCopy(result + offset, line_buf, len * kCharSize);
    offset += len;
  }
  DCHECK_NOT_NULL(result);
  result[offset] = '\0';
  return result;
}

namespace {

std::vector<char> ReadCharsFromFile(FILE* file, bool* exists, bool verbose,
                                    const char* filename) {
  if (file == nullptr || fseek(file, 0, SEEK_END) != 0) {
    if (verbose) {
      base::OS::PrintError("Cannot read from file %s.\n", filename);
    }
    *exists = false;
    return std::vector<char>();
  }

  // Get the size of the file and rewind it.
  ptrdiff_t size = ftell(file);
  rewind(file);

  std::vector<char> result(size);
  for (ptrdiff_t i = 0; i < size && feof(file) == 0;) {
    ptrdiff_t read = fread(result.data() + i, 1, size - i, file);
    if (read != (size - i) && ferror(file) != 0) {
      base::Fclose(file);
      *exists = false;
      return std::vector<char>();
    }
    i += read;
  }
  *exists = true;
  return result;
}

std::vector<char> ReadCharsFromFile(const char* filename, bool* exists,
                                    bool verbose) {
  FILE* file = base::OS::FOpen(filename, "rb");
  std::vector<char> result = ReadCharsFromFile(file, exists, verbose, filename);
  if (file != nullptr) base::Fclose(file);
  return result;
}

std::string VectorToString(const std::vector<char>& chars) {
  if (chars.empty()) {
    return std::string();
  }
  return std::string(chars.begin(), chars.end());
}

int WriteCharsToFile(const char* str, int size, FILE* f) {
  int total = 0;
  while (total < size) {
    int write = static_cast<int>(fwrite(str, 1, size - total, f));
    if (write == 0) {
      return total;
    }
    total += write;
    str += write;
  }
  return total;
}

}  // namespace

std::string ReadFile(const char* filename, bool* exists, bool verbose) {
  std::vector<char> result = ReadCharsFromFile(filename, exists, verbose);
  return VectorToString(result);
}

std::string ReadFile(FILE* file, bool* exists, bool verbose) {
  std::vector<char> result = ReadCharsFromFile(file, exists, verbose, "");
  return VectorToString(result);
}

int WriteChars(const char* filename, const char* str, int size, bool verbose) {
  FILE* f = base::OS::FOpen(filename, "wb");
  if (f == nullptr) {
    if (verbose) {
      base::OS::PrintError("Cannot open file %s for writing.\n", filename);
    }
    return 0;
  }
  int written = WriteCharsToFile(str, size, f);
  base::Fclose(f);
  return written;
}

int WriteBytes(const char* filename, const byte* bytes, int size,
               bool verbose) {
  const char* str = reinterpret_cast<const char*>(bytes);
  return WriteChars(filename, str, size, verbose);
}

void StringBuilder::AddFormatted(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  AddFormattedList(format, arguments);
  va_end(arguments);
}

void StringBuilder::AddFormattedList(const char* format, va_list list) {
  DCHECK(!is_finalized() && position_ <= buffer_.length());
  int n = VSNPrintF(buffer_ + position_, format, list);
  if (n < 0 || n >= (buffer_.length() - position_)) {
    position_ = buffer_.length();
  } else {
    position_ += n;
  }
}

// Returns false iff d is NaN, +0, or -0.
bool DoubleToBoolean(double d) {
  IeeeDoubleArchType u;
  u.d = d;
  if (u.bits.exp == 2047) {
    // Detect NaN for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) != 0) return false;
  }
  if (u.bits.exp == 0) {
    // Detect +0, and -0 for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) == 0) return false;
  }
  return true;
}

uintptr_t GetCurrentStackPosition() {
#if V8_CC_MSVC
  return reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());
#else
  return reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#endif
}

// The filter is a pattern that matches function names in this way:
//   "*"      all; the default
//   "-"      all but the top-level function
//   "-name"  all but the function "name"
//   ""       only the top-level function
//   "name"   only the function "name"
//   "name*"  only functions starting with "name"
//   "~"      none; the tilde is not an identifier
bool PassesFilter(Vector<const char> name, Vector<const char> filter) {
  if (filter.size() == 0) return name.size() == 0;
  auto filter_it = filter.begin();
  bool positive_filter = true;
  if (*filter_it == '-') {
    ++filter_it;
    positive_filter = false;
  }
  if (filter_it == filter.end()) return name.size() != 0;
  if (*filter_it == '*') return positive_filter;
  if (*filter_it == '~') return !positive_filter;

  bool prefix_match = filter[filter.size() - 1] == '*';
  size_t min_match_length = filter.size();
  if (!positive_filter) min_match_length--;  // Subtract 1 for leading '-'.
  if (prefix_match) min_match_length--;      // Subtract 1 for trailing '*'.

  if (name.size() < min_match_length) return !positive_filter;

  // TODO(sigurds): Use the new version of std::mismatch here, once we
  // can assume C++14.
  auto res = std::mismatch(filter_it, filter.end(), name.begin());
  if (res.first == filter.end()) {
    if (res.second == name.end()) {
      // The strings match, so {name} passes if we have a {positive_filter}.
      return positive_filter;
    }
    // {name} is longer than the filter, so {name} passes if we don't have a
    // {positive_filter}.
    return !positive_filter;
  }
  if (*res.first == '*') {
    // We matched up to the wildcard, so {name} passes if we have a
    // {positive_filter}.
    return positive_filter;
  }
  // We don't match, so {name} passes if we don't have a {positive_filter}.
  return !positive_filter;
}

}  // namespace internal
}  // namespace v8
