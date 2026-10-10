//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of FileMode class. This is the class
/// that handles everything related to a file's mode.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_FILE_MODE_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_FILE_MODE_H

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// FileMode class handles everything regarding the mode of the file, be it
// opening mode or content type.
class FileMode {
public:
  // FileMode constructor accepts the mode string as an argument.
  // It performs validation against several rules and records the `file_mode`
  // property to the specific mode.
  constexpr FileMode(const char *mode) : file_mode_(0) {
    // First character in |mode| should be 'a', 'r' or 'w'.
    if (*mode != 'a' && *mode != 'r' && *mode != 'w')
      return;

    // There should be exactly one main mode ('a', 'r' or 'w') character.
    // If there are more than one main mode characters listed, then
    // we will consider |mode| as incorrect and set the file's mode to zero
    // meaning the file's mode is in an invalid state.;
    int main_mode_count = 0;

    for (; *mode != '\0'; ++mode) {
      switch (*mode) {
      case 'r':
        file_mode_ |= static_cast<Mode>(OpenMode::READ);
        ++main_mode_count;
        break;
      case 'w':
        file_mode_ |= static_cast<Mode>(OpenMode::WRITE);
        ++main_mode_count;
        break;
      case '+':
        file_mode_ |= static_cast<Mode>(OpenMode::PLUS);
        break;
      case 'b':
        file_mode_ |= static_cast<Mode>(ContentType::BINARY);
        break;
      case 'a':
        file_mode_ |= static_cast<Mode>(OpenMode::APPEND);
        ++main_mode_count;
        break;
      case 'x':
        file_mode_ |= static_cast<Mode>(CreateType::EXCLUSIVE);
        break;
      default:
        file_mode_ = 0;
      }
    }

    if (main_mode_count != 1)
      file_mode_ = 0;
  }

  static const FileMode APPEND_MODE;
  static const FileMode READ_MODE;
  static const FileMode WRITE_MODE;

  constexpr bool write_allowed() const {
    return is_write() || is_append() || is_update();
  }

  constexpr bool read_allowed() const { return is_read() || is_update(); }

  constexpr bool is_valid() const { return file_mode_ != 0; }

  constexpr bool is_append() const {
    return (file_mode_ & static_cast<Mode>(OpenMode::APPEND)) != 0;
  }

  constexpr bool is_update() const {
    return (file_mode_ & static_cast<Mode>(OpenMode::PLUS)) != 0;
  }

  constexpr bool is_write() const {
    return (file_mode_ & static_cast<Mode>(OpenMode::WRITE)) != 0;
  }

  constexpr bool is_read() const {
    return (file_mode_ & static_cast<Mode>(OpenMode::READ)) != 0;
  }

  constexpr bool is_binary_format() const {
    return (file_mode_ & static_cast<Mode>(ContentType::BINARY)) != 0;
  }

  // checks if a file was created for writing
  constexpr bool is_exclusive_create() const {
    return (file_mode_ & static_cast<Mode>(CreateType::EXCLUSIVE)) != 0;
  }

private:
  // Mode is a generic or abstract mode bit for all kinds of modes
  // (open-mode, 'content-mode', 'create-modes')
  using Mode = uint32_t;

  // Denotes the mode of the file.
  //
  // The three different types of flags below are to be used with '|' operator.
  // Their values correspond to mutually exclusive bits in a 32-bit unsigned
  // integer value. A flag set can include both READ and WRITE if the file
  // is opened in update mode (ie. if the file was opened with a '+' the mode
  // string.)
  enum class OpenMode : Mode {
    READ = 0x1,
    WRITE = 0x2,
    APPEND = 0x4,
    PLUS = 0x8,
  };

  // Denotes a file opened in binary mode (which is specified by including
  // the 'b' character in the mode string.)
  enum class ContentType : Mode {
    BINARY = 0x10,
  };

  // Denotes a file to be created for writing.
  enum class CreateType : Mode {
    EXCLUSIVE = 0x100,
  };

  // This property tracks the mode for the particular file instance (i.e
  // currently opened file)
  Mode file_mode_;
};

LIBC_INLINE_VAR constexpr FileMode FileMode::APPEND_MODE("a");
LIBC_INLINE_VAR constexpr FileMode FileMode::READ_MODE("r");
LIBC_INLINE_VAR constexpr FileMode FileMode::WRITE_MODE("w");

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_FILE_MODE_H
