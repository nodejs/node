// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPRESSION_CLASSIFIER_H
#define V8_EXPRESSION_CLASSIFIER_H

#include "src/v8.h"

#include "src/messages.h"
#include "src/scanner.h"
#include "src/token.h"

namespace v8 {
namespace internal {


class ExpressionClassifier {
 public:
  struct Error {
    Error()
        : location(Scanner::Location::invalid()),
          message(MessageTemplate::kNone),
          arg(nullptr) {}

    Scanner::Location location;
    MessageTemplate::Template message;
    const char* arg;
  };

  enum TargetProduction {
    ExpressionProduction = 1 << 0,
    BindingPatternProduction = 1 << 1,
    AssignmentPatternProduction = 1 << 2,
    DistinctFormalParametersProduction = 1 << 3,
    StrictModeFormalParametersProduction = 1 << 4,
    StrongModeFormalParametersProduction = 1 << 5,
    ArrowFormalParametersProduction = 1 << 6,

    PatternProductions =
        (BindingPatternProduction | AssignmentPatternProduction),
    FormalParametersProductions = (DistinctFormalParametersProduction |
                                   StrictModeFormalParametersProduction |
                                   StrongModeFormalParametersProduction),
    StandardProductions = ExpressionProduction | PatternProductions,
    AllProductions = (StandardProductions | FormalParametersProductions |
                      ArrowFormalParametersProduction)
  };

  ExpressionClassifier()
      : invalid_productions_(0), duplicate_finder_(nullptr) {}

  explicit ExpressionClassifier(DuplicateFinder* duplicate_finder)
      : invalid_productions_(0), duplicate_finder_(duplicate_finder) {}

  bool is_valid(unsigned productions) const {
    return (invalid_productions_ & productions) == 0;
  }

  DuplicateFinder* duplicate_finder() const { return duplicate_finder_; }

  bool is_valid_expression() const { return is_valid(ExpressionProduction); }

  bool is_valid_binding_pattern() const {
    return is_valid(BindingPatternProduction);
  }

  bool is_valid_assignment_pattern() const {
    return is_valid(AssignmentPatternProduction);
  }

  bool is_valid_arrow_formal_parameters() const {
    return is_valid(ArrowFormalParametersProduction);
  }

  bool is_valid_formal_parameter_list_without_duplicates() const {
    return is_valid(DistinctFormalParametersProduction);
  }

  // Note: callers should also check
  // is_valid_formal_parameter_list_without_duplicates().
  bool is_valid_strict_mode_formal_parameters() const {
    return is_valid(StrictModeFormalParametersProduction);
  }

  // Note: callers should also check is_valid_strict_mode_formal_parameters()
  // and is_valid_formal_parameter_list_without_duplicates().
  bool is_valid_strong_mode_formal_parameters() const {
    return is_valid(StrongModeFormalParametersProduction);
  }

  const Error& expression_error() const { return expression_error_; }

  const Error& binding_pattern_error() const { return binding_pattern_error_; }

  const Error& assignment_pattern_error() const {
    return assignment_pattern_error_;
  }

  const Error& arrow_formal_parameters_error() const {
    return arrow_formal_parameters_error_;
  }

  const Error& duplicate_formal_parameter_error() const {
    return duplicate_formal_parameter_error_;
  }

  const Error& strict_mode_formal_parameter_error() const {
    return strict_mode_formal_parameter_error_;
  }

  const Error& strong_mode_formal_parameter_error() const {
    return strong_mode_formal_parameter_error_;
  }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!is_valid_expression()) return;
    invalid_productions_ |= ExpressionProduction;
    expression_error_.location = loc;
    expression_error_.message = message;
    expression_error_.arg = arg;
  }

  void RecordBindingPatternError(const Scanner::Location& loc,
                                 MessageTemplate::Template message,
                                 const char* arg = nullptr) {
    if (!is_valid_binding_pattern()) return;
    invalid_productions_ |= BindingPatternProduction;
    binding_pattern_error_.location = loc;
    binding_pattern_error_.message = message;
    binding_pattern_error_.arg = arg;
  }

  void RecordAssignmentPatternError(const Scanner::Location& loc,
                                    MessageTemplate::Template message,
                                    const char* arg = nullptr) {
    if (!is_valid_assignment_pattern()) return;
    invalid_productions_ |= AssignmentPatternProduction;
    assignment_pattern_error_.location = loc;
    assignment_pattern_error_.message = message;
    assignment_pattern_error_.arg = arg;
  }

  void RecordArrowFormalParametersError(const Scanner::Location& loc,
                                        MessageTemplate::Template message,
                                        const char* arg = nullptr) {
    if (!is_valid_arrow_formal_parameters()) return;
    invalid_productions_ |= ArrowFormalParametersProduction;
    arrow_formal_parameters_error_.location = loc;
    arrow_formal_parameters_error_.message = message;
    arrow_formal_parameters_error_.arg = arg;
  }

  void RecordDuplicateFormalParameterError(const Scanner::Location& loc) {
    if (!is_valid_formal_parameter_list_without_duplicates()) return;
    invalid_productions_ |= DistinctFormalParametersProduction;
    duplicate_formal_parameter_error_.location = loc;
    duplicate_formal_parameter_error_.message =
        MessageTemplate::kStrictParamDupe;
    duplicate_formal_parameter_error_.arg = nullptr;
  }

  // Record a binding that would be invalid in strict mode.  Confusingly this
  // is not the same as StrictFormalParameterList, which simply forbids
  // duplicate bindings.
  void RecordStrictModeFormalParameterError(const Scanner::Location& loc,
                                            MessageTemplate::Template message,
                                            const char* arg = nullptr) {
    if (!is_valid_strict_mode_formal_parameters()) return;
    invalid_productions_ |= StrictModeFormalParametersProduction;
    strict_mode_formal_parameter_error_.location = loc;
    strict_mode_formal_parameter_error_.message = message;
    strict_mode_formal_parameter_error_.arg = arg;
  }

  void RecordStrongModeFormalParameterError(const Scanner::Location& loc,
                                            MessageTemplate::Template message,
                                            const char* arg = nullptr) {
    if (!is_valid_strong_mode_formal_parameters()) return;
    invalid_productions_ |= StrongModeFormalParametersProduction;
    strong_mode_formal_parameter_error_.location = loc;
    strong_mode_formal_parameter_error_.message = message;
    strong_mode_formal_parameter_error_.arg = arg;
  }

  void Accumulate(const ExpressionClassifier& inner,
                  unsigned productions = StandardProductions) {
    // Propagate errors from inner, but don't overwrite already recorded
    // errors.
    unsigned non_arrow_inner_invalid_productions =
        inner.invalid_productions_ & ~ArrowFormalParametersProduction;
    if (non_arrow_inner_invalid_productions == 0) return;
    unsigned non_arrow_productions =
        productions & ~ArrowFormalParametersProduction;
    unsigned errors =
        non_arrow_productions & non_arrow_inner_invalid_productions;
    errors &= ~invalid_productions_;
    if (errors != 0) {
      invalid_productions_ |= errors;
      if (errors & ExpressionProduction)
        expression_error_ = inner.expression_error_;
      if (errors & BindingPatternProduction)
        binding_pattern_error_ = inner.binding_pattern_error_;
      if (errors & AssignmentPatternProduction)
        assignment_pattern_error_ = inner.assignment_pattern_error_;
      if (errors & DistinctFormalParametersProduction)
        duplicate_formal_parameter_error_ =
            inner.duplicate_formal_parameter_error_;
      if (errors & StrictModeFormalParametersProduction)
        strict_mode_formal_parameter_error_ =
            inner.strict_mode_formal_parameter_error_;
      if (errors & StrongModeFormalParametersProduction)
        strong_mode_formal_parameter_error_ =
            inner.strong_mode_formal_parameter_error_;
    }

    // As an exception to the above, the result continues to be a valid arrow
    // formal parameters if the inner expression is a valid binding pattern.
    if (productions & ArrowFormalParametersProduction &&
        is_valid_arrow_formal_parameters() &&
        !inner.is_valid_binding_pattern()) {
      invalid_productions_ |= ArrowFormalParametersProduction;
      arrow_formal_parameters_error_ = inner.binding_pattern_error_;
    }
  }

  void AccumulateReclassifyingAsPattern(const ExpressionClassifier& inner) {
    Accumulate(inner, AllProductions & ~PatternProductions);
    if (!inner.is_valid_expression()) {
      if (is_valid_binding_pattern()) {
        binding_pattern_error_ = inner.expression_error();
      }
      if (is_valid_assignment_pattern()) {
        assignment_pattern_error_ = inner.expression_error();
      }
    }
  }

 private:
  unsigned invalid_productions_;
  Error expression_error_;
  Error binding_pattern_error_;
  Error assignment_pattern_error_;
  Error arrow_formal_parameters_error_;
  Error duplicate_formal_parameter_error_;
  Error strict_mode_formal_parameter_error_;
  Error strong_mode_formal_parameter_error_;
  DuplicateFinder* duplicate_finder_;
};
}
}  // v8::internal

#endif  // V8_EXPRESSION_CLASSIFIER_H
