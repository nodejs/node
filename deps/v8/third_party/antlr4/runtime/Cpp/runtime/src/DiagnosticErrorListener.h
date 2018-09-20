/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "BaseErrorListener.h"

namespace antlr4 {

/// <summary>
/// This implementation of <seealso cref="ANTLRErrorListener"/> can be used to
/// identify certain potential correctness and performance problems in grammars.
/// "Reports" are made by calling <seealso cref="Parser#notifyErrorListeners"/>
/// with the appropriate message.
///
/// <ul>
/// <li><b>Ambiguities</b>: These are cases where more than one path through the
/// grammar can match the input.</li>
/// <li><b>Weak context sensitivity</b>: These are cases where full-context
/// prediction resolved an SLL conflict to a unique alternative which equaled
/// the minimum alternative of the SLL conflict.</li> <li><b>Strong (forced)
/// context sensitivity</b>: These are cases where the full-context prediction
/// resolved an SLL conflict to a unique alternative, <em>and</em> the minimum
/// alternative of the SLL conflict was found to not be a truly viable
/// alternative. Two-stage parsing cannot be used for inputs where this
/// situation occurs.</li>
/// </ul>
///
/// @author Sam Harwell
/// </summary>
class ANTLR4CPP_PUBLIC DiagnosticErrorListener : public BaseErrorListener {
  /// <summary>
  /// When {@code true}, only exactly known ambiguities are reported.
  /// </summary>
 protected:
  const bool exactOnly;

  /// <summary>
  /// Initializes a new instance of <seealso cref="DiagnosticErrorListener"/>
  /// which only reports exact ambiguities.
  /// </summary>
 public:
  DiagnosticErrorListener();

  /// <summary>
  /// Initializes a new instance of <seealso cref="DiagnosticErrorListener"/>,
  /// specifying whether all ambiguities or only exact ambiguities are reported.
  /// </summary>
  /// <param name="exactOnly"> {@code true} to report only exact ambiguities,
  /// otherwise
  /// {@code false} to report all ambiguities. </param>
  DiagnosticErrorListener(bool exactOnly);

  virtual void reportAmbiguity(Parser* recognizer, const dfa::DFA& dfa,
                               size_t startIndex, size_t stopIndex, bool exact,
                               const antlrcpp::BitSet& ambigAlts,
                               atn::ATNConfigSet* configs) override;

  virtual void reportAttemptingFullContext(
      Parser* recognizer, const dfa::DFA& dfa, size_t startIndex,
      size_t stopIndex, const antlrcpp::BitSet& conflictingAlts,
      atn::ATNConfigSet* configs) override;

  virtual void reportContextSensitivity(Parser* recognizer, const dfa::DFA& dfa,
                                        size_t startIndex, size_t stopIndex,
                                        size_t prediction,
                                        atn::ATNConfigSet* configs) override;

 protected:
  virtual std::string getDecisionDescription(Parser* recognizer,
                                             const dfa::DFA& dfa);

  /// <summary>
  /// Computes the set of conflicting or ambiguous alternatives from a
  /// configuration set, if that information was not already provided by the
  /// parser.
  /// </summary>
  /// <param name="reportedAlts"> The set of conflicting or ambiguous
  /// alternatives, as reported by the parser. </param> <param name="configs">
  /// The conflicting or ambiguous configuration set. </param> <returns> Returns
  /// {@code reportedAlts} if it is not {@code null}, otherwise returns the set
  /// of alternatives represented in {@code configs}. </returns>
  virtual antlrcpp::BitSet getConflictingAlts(
      const antlrcpp::BitSet& reportedAlts, atn::ATNConfigSet* configs);
};

}  // namespace antlr4
