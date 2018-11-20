/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace atn {

/// Represents the type of recognizer an ATN applies to.
enum class ATNType {
  LEXER = 0,
  PARSER = 1,
};

}  // namespace atn
}  // namespace antlr4
