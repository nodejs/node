//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Regular expression parser interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

// Parses an Extended Regular Expression (ERE) into the given ExprPool.
// Optionally populates out_re_nsub with the count of parenthesized
// subexpressions.
ErrorOr<ExprId> parse_ere(cpp::string_view pattern, int cflags, ExprPool &pool,
                          size_t *out_re_nsub = nullptr);

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_PARSER_H
