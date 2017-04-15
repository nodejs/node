// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils.h"

#include <stdarg.h>
#include <sys/stat.h>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {


SimpleStringBuilder::SimpleStringBuilder(int size) {
  buffer_ = Vector<char>::New(size);
  position_ = 0;
}


void SimpleStringBuilder::AddString(const char* s) {
  AddSubstring(s, StrLength(s));
}


void SimpleStringBuilder::AddSubstring(const char* s, int n) {
  DCHECK(!is_finalized() && position_ + n <= buffer_.length());
  DCHECK(static_cast<size_t>(n) <= strlen(s));
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
  DCHECK(strlen(buffer_.start()) == static_cast<size_t>(position_));
  position_ = -1;
  DCHECK(is_finalized());
  return buffer_.start();
}


std::ostream& operator<<(std::ostream& os, FeedbackVectorSlot slot) {
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
  return base::OS::VSNPrintF(str.start(), str.length(), format, args);
}


void StrNCpy(Vector<char> dest, const char* src, size_t n) {
  base::OS::StrNCpy(dest.start(), dest.length(), src, n);
}


void Flush(FILE* out) {
  fflush(out);
}


char* ReadLine(const char* prompt) {
  char* result = NULL;
  char line_buf[256];
  int offset = 0;
  bool keep_going = true;
  fprintf(stdout, "%s", prompt);
  fflush(stdout);
  while (keep_going) {
    if (fgets(line_buf, sizeof(line_buf), stdin) == NULL) {
      // fgets got an error. Just give up.
      if (result != NULL) {
        DeleteArray(result);
      }
      return NULL;
    }
    int len = StrLength(line_buf);
    if (len > 1 &&
        line_buf[len - 2] == '\\' &&
        line_buf[len - 1] == '\n') {
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
    if (result == NULL) {
      // Allocate the initial result and make room for the terminating '\0'
      result = NewArray<char>(len + 1);
    } else {
      // Allocate a new result with enough room for the new addition.
      int new_len = offset + len + 1;
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
  DCHECK(result != NULL);
  result[offset] = '\0';
  return result;
}


char* ReadCharsFromFile(FILE* file,
                        int* size,
                        int extra_space,
                        bool verbose,
                        const char* filename) {
  if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
    if (verbose) {
      base::OS::PrintError("Cannot read from file %s.\n", filename);
    }
    return NULL;
  }

  // Get the size of the file and rewind it.
  *size = static_cast<int>(ftell(file));
  rewind(file);

  char* result = NewArray<char>(*size + extra_space);
  for (int i = 0; i < *size && feof(file) == 0;) {
    int read = static_cast<int>(fread(&result[i], 1, *size - i, file));
    if (read != (*size - i) && ferror(file) != 0) {
      fclose(file);
      DeleteArray(result);
      return NULL;
    }
    i += read;
  }
  return result;
}


char* ReadCharsFromFile(const char* filename,
                        int* size,
                        int extra_space,
                        bool verbose) {
  FILE* file = base::OS::FOpen(filename, "rb");
  char* result = ReadCharsFromFile(file, size, extra_space, verbose, filename);
  if (file != NULL) fclose(file);
  return result;
}


byte* ReadBytes(const char* filename, int* size, bool verbose) {
  char* chars = ReadCharsFromFile(filename, size, 0, verbose);
  return reinterpret_cast<byte*>(chars);
}


static Vector<const char> SetVectorContents(char* chars,
                                            int size,
                                            bool* exists) {
  if (!chars) {
    *exists = false;
    return Vector<const char>::empty();
  }
  chars[size] = '\0';
  *exists = true;
  return Vector<const char>(chars, size);
}


Vector<const char> ReadFile(const char* filename,
                            bool* exists,
                            bool verbose) {
  int size;
  char* result = ReadCharsFromFile(filename, &size, 1, verbose);
  return SetVectorContents(result, size, exists);
}


Vector<const char> ReadFile(FILE* file,
                            bool* exists,
                            bool verbose) {
  int size;
  char* result = ReadCharsFromFile(file, &size, 1, verbose, "");
  return SetVectorContents(result, size, exists);
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


int AppendChars(const char* filename,
                const char* str,
                int size,
                bool verbose) {
  FILE* f = base::OS::FOpen(filename, "ab");
  if (f == NULL) {
    if (verbose) {
      base::OS::PrintError("Cannot open file %s for writing.\n", filename);
    }
    return 0;
  }
  int written = WriteCharsToFile(str, size, f);
  fclose(f);
  return written;
}


int WriteChars(const char* filename,
               const char* str,
               int size,
               bool verbose) {
  FILE* f = base::OS::FOpen(filename, "wb");
  if (f == NULL) {
    if (verbose) {
      base::OS::PrintError("Cannot open file %s for writing.\n", filename);
    }
    return 0;
  }
  int written = WriteCharsToFile(str, size, f);
  fclose(f);
  return written;
}


int WriteBytes(const char* filename,
               const byte* bytes,
               int size,
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


#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
static void MemMoveWrapper(void* dest, const void* src, size_t size) {
  memmove(dest, src, size);
}


// Initialize to library version so we can call this at any time during startup.
static MemMoveFunction memmove_function = &MemMoveWrapper;

// Defined in codegen-ia32.cc.
MemMoveFunction CreateMemMoveFunction(Isolate* isolate);

// Copy memory area to disjoint memory area.
void MemMove(void* dest, const void* src, size_t size) {
  if (size == 0) return;
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  (*memmove_function)(dest, src, size);
}

#elif V8_OS_POSIX && V8_HOST_ARCH_ARM
void MemCopyUint16Uint8Wrapper(uint16_t* dest, const uint8_t* src,
                               size_t chars) {
  uint16_t* limit = dest + chars;
  while (dest < limit) {
    *dest++ = static_cast<uint16_t>(*src++);
  }
}

V8_EXPORT_PRIVATE MemCopyUint8Function memcopy_uint8_function =
    &MemCopyUint8Wrapper;
MemCopyUint16Uint8Function memcopy_uint16_uint8_function =
    &MemCopyUint16Uint8Wrapper;
// Defined in codegen-arm.cc.
MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub);
MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
    Isolate* isolate, MemCopyUint16Uint8Function stub);

#elif V8_OS_POSIX && V8_HOST_ARCH_MIPS
V8_EXPORT_PRIVATE MemCopyUint8Function memcopy_uint8_function =
    &MemCopyUint8Wrapper;
// Defined in codegen-mips.cc.
MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub);
#endif


static bool g_memcopy_functions_initialized = false;


void init_memcopy_functions(Isolate* isolate) {
  if (g_memcopy_functions_initialized) return;
  g_memcopy_functions_initialized = true;
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
  MemMoveFunction generated_memmove = CreateMemMoveFunction(isolate);
  if (generated_memmove != NULL) {
    memmove_function = generated_memmove;
  }
#elif V8_OS_POSIX && V8_HOST_ARCH_ARM
  memcopy_uint8_function =
      CreateMemCopyUint8Function(isolate, &MemCopyUint8Wrapper);
  memcopy_uint16_uint8_function =
      CreateMemCopyUint16Uint8Function(isolate, &MemCopyUint16Uint8Wrapper);
#elif V8_OS_POSIX && V8_HOST_ARCH_MIPS
  memcopy_uint8_function =
      CreateMemCopyUint8Function(isolate, &MemCopyUint8Wrapper);
#endif
}


bool DoubleToBoolean(double d) {
  // NaN, +0, and -0 should return the false object
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


}  // namespace internal
}  // namespace v8
