/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "BaseErrorListener.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC ConsoleErrorListener : public BaseErrorListener {
 public:
  /**
   * Provides a default instance of {@link ConsoleErrorListener}.
   */
  static ConsoleErrorListener INSTANCE;

  /**
   * {@inheritDoc}
   *
   * <p>
   * This implementation prints messages to {@link System#err} containing the
   * values of {@code line}, {@code charPositionInLine}, and {@code msg} using
   * the following format.</p>
   *
   * <pre>
   * line <em>line</em>:<em>charPositionInLine</em> <em>msg</em>
   * </pre>
   */
  virtual void syntaxError(Recognizer* recognizer, Token* offendingSymbol,
                           size_t line, size_t charPositionInLine,
                           const std::string& msg,
                           std::exception_ptr e) override;
};

}  // namespace antlr4
