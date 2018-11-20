/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "RecognitionException.h"

namespace antlr4 {

/// <summary>
/// This signifies any kind of mismatched input exceptions such as
///  when the current input does not match the expected token.
/// </summary>
class ANTLR4CPP_PUBLIC InputMismatchException : public RecognitionException {
 public:
  InputMismatchException(Parser* recognizer);
  InputMismatchException(InputMismatchException const&) = default;
  ~InputMismatchException();
  InputMismatchException& operator=(InputMismatchException const&) = default;
};

}  // namespace antlr4
