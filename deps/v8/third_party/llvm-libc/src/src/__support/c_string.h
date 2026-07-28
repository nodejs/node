//===-- Implementation of a struct to hold a string in menory -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_C_STRING_H
#define LLVM_LIBC_SRC___SUPPORT_C_STRING_H

#include "src/__support/CPP/string.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/attributes.h" // for LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// The CString class is a companion to the cpp::string class. Its use case is as
// a return value for a function that in C would return a char* and a flag for
// if that char* needs to be freed.
class CString {
  cpp::string str;

public:
  // These constructors can be implemented iff required.
  CString() = delete;
  CString(const CString &) = delete;
  CString(CString &&) = delete;

  LIBC_INLINE CString(cpp::string in_str) : str(in_str) {}

  LIBC_INLINE operator const char *() const { return str.c_str(); }
  LIBC_INLINE operator cpp::string_view() const { return str; }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_C_STRING_H
