// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSING_H_
#define V8_PARSING_PARSING_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class ParseInfo;

namespace parsing {

// Parses the top-level source code represented by the parse info and sets its
// function literal.  Returns false (and deallocates any allocated AST
// nodes) if parsing failed.
V8_EXPORT_PRIVATE bool ParseProgram(ParseInfo* info);

// Like ParseProgram but for an individual function.
V8_EXPORT_PRIVATE bool ParseFunction(ParseInfo* info);

// If you don't know whether info->is_toplevel() is true or not, use this method
// to dispatch to either of the above functions. Prefer to use the above methods
// whenever possible.
V8_EXPORT_PRIVATE bool ParseAny(ParseInfo* info);

}  // namespace parsing
}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSING_H_
