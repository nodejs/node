/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionEventInfo.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// This class represents profiling event information for a syntax error
/// identified during prediction. Syntax errors occur when the prediction
/// algorithm is unable to identify an alternative which would lead to a
/// successful parse.
/// </summary>
/// <seealso cref= Parser#notifyErrorListeners(Token, String,
/// RecognitionException) </seealso> <seealso cref=
/// ANTLRErrorListener#syntaxError
///
/// @since 4.3 </seealso>
class ANTLR4CPP_PUBLIC ErrorInfo : public DecisionEventInfo {
 public:
  /// <summary>
  /// Constructs a new instance of the <seealso cref="ErrorInfo"/> class with
  /// the specified detailed syntax error information.
  /// </summary>
  /// <param name="decision"> The decision number </param>
  /// <param name="configs"> The final configuration set reached during
  /// prediction prior to reaching the <seealso cref="ATNSimulator#ERROR"/>
  /// state </param> <param name="input"> The input token stream </param> <param
  /// name="startIndex"> The start index for the current prediction </param>
  /// <param name="stopIndex"> The index at which the syntax error was
  /// identified </param> <param name="fullCtx"> {@code true} if the syntax
  /// error was identified during LL prediction; otherwise, {@code false} if the
  /// syntax error was identified during SLL prediction </param>
  ErrorInfo(size_t decision, ATNConfigSet* configs, TokenStream* input,
            size_t startIndex, size_t stopIndex, bool fullCtx);
};

}  // namespace atn
}  // namespace antlr4
