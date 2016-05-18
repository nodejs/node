// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_EXPRESSION_CLASSIFIER_H
#define V8_PARSING_EXPRESSION_CLASSIFIER_H

#include "src/messages.h"
#include "src/parsing/scanner.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {


template <typename Traits>
class ExpressionClassifier {
 public:
  struct Error {
    Error()
        : location(Scanner::Location::invalid()),
          message(MessageTemplate::kNone),
          type(kSyntaxError),
          arg(nullptr) {}

    Scanner::Location location;
    MessageTemplate::Template message : 30;
    ParseErrorType type : 2;
    const char* arg;
  };

  enum TargetProduction {
    ExpressionProduction = 1 << 0,
    FormalParameterInitializerProduction = 1 << 1,
    BindingPatternProduction = 1 << 2,
    AssignmentPatternProduction = 1 << 3,
    DistinctFormalParametersProduction = 1 << 4,
    StrictModeFormalParametersProduction = 1 << 5,
    StrongModeFormalParametersProduction = 1 << 6,
    ArrowFormalParametersProduction = 1 << 7,
    LetPatternProduction = 1 << 8,
    CoverInitializedNameProduction = 1 << 9,

    ExpressionProductions =
        (ExpressionProduction | FormalParameterInitializerProduction),
    PatternProductions = (BindingPatternProduction |
                          AssignmentPatternProduction | LetPatternProduction),
    FormalParametersProductions = (DistinctFormalParametersProduction |
                                   StrictModeFormalParametersProduction |
                                   StrongModeFormalParametersProduction),
    StandardProductions = ExpressionProductions | PatternProductions,
    AllProductions =
        (StandardProductions | FormalParametersProductions |
         ArrowFormalParametersProduction | CoverInitializedNameProduction)
  };

  enum FunctionProperties { NonSimpleParameter = 1 << 0 };

  explicit ExpressionClassifier(const Traits* t)
      : zone_(t->zone()),
        non_patterns_to_rewrite_(t->GetNonPatternList()),
        invalid_productions_(0),
        function_properties_(0),
        duplicate_finder_(nullptr) {
    non_pattern_begin_ = non_patterns_to_rewrite_->length();
  }

  ExpressionClassifier(const Traits* t, DuplicateFinder* duplicate_finder)
      : zone_(t->zone()),
        non_patterns_to_rewrite_(t->GetNonPatternList()),
        invalid_productions_(0),
        function_properties_(0),
        duplicate_finder_(duplicate_finder) {
    non_pattern_begin_ = non_patterns_to_rewrite_->length();
  }

  ~ExpressionClassifier() { Discard(); }

  bool is_valid(unsigned productions) const {
    return (invalid_productions_ & productions) == 0;
  }

  DuplicateFinder* duplicate_finder() const { return duplicate_finder_; }

  bool is_valid_expression() const { return is_valid(ExpressionProduction); }

  bool is_valid_formal_parameter_initializer() const {
    return is_valid(FormalParameterInitializerProduction);
  }

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

  bool is_valid_let_pattern() const { return is_valid(LetPatternProduction); }

  const Error& expression_error() const { return expression_error_; }

  const Error& formal_parameter_initializer_error() const {
    return formal_parameter_initializer_error_;
  }

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

  const Error& let_pattern_error() const { return let_pattern_error_; }

  bool has_cover_initialized_name() const {
    return !is_valid(CoverInitializedNameProduction);
  }
  const Error& cover_initialized_name_error() const {
    return cover_initialized_name_error_;
  }

  bool is_simple_parameter_list() const {
    return !(function_properties_ & NonSimpleParameter);
  }

  void RecordNonSimpleParameter() {
    function_properties_ |= NonSimpleParameter;
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

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             ParseErrorType type, const char* arg = nullptr) {
    if (!is_valid_expression()) return;
    invalid_productions_ |= ExpressionProduction;
    expression_error_.location = loc;
    expression_error_.message = message;
    expression_error_.arg = arg;
    expression_error_.type = type;
  }

  void RecordFormalParameterInitializerError(const Scanner::Location& loc,
                                             MessageTemplate::Template message,
                                             const char* arg = nullptr) {
    if (!is_valid_formal_parameter_initializer()) return;
    invalid_productions_ |= FormalParameterInitializerProduction;
    formal_parameter_initializer_error_.location = loc;
    formal_parameter_initializer_error_.message = message;
    formal_parameter_initializer_error_.arg = arg;
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

  void RecordPatternError(const Scanner::Location& loc,
                          MessageTemplate::Template message,
                          const char* arg = nullptr) {
    RecordBindingPatternError(loc, message, arg);
    RecordAssignmentPatternError(loc, message, arg);
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
    duplicate_formal_parameter_error_.message = MessageTemplate::kParamDupe;
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

  void RecordLetPatternError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!is_valid_let_pattern()) return;
    invalid_productions_ |= LetPatternProduction;
    let_pattern_error_.location = loc;
    let_pattern_error_.message = message;
    let_pattern_error_.arg = arg;
  }

  void RecordCoverInitializedNameError(const Scanner::Location& loc,
                                       MessageTemplate::Template message,
                                       const char* arg = nullptr) {
    if (has_cover_initialized_name()) return;
    invalid_productions_ |= CoverInitializedNameProduction;
    cover_initialized_name_error_.location = loc;
    cover_initialized_name_error_.message = message;
    cover_initialized_name_error_.arg = arg;
  }

  void ForgiveCoverInitializedNameError() {
    invalid_productions_ &= ~CoverInitializedNameProduction;
    cover_initialized_name_error_ = Error();
  }

  void ForgiveAssignmentPatternError() {
    invalid_productions_ &= ~AssignmentPatternProduction;
    assignment_pattern_error_ = Error();
  }

  void Accumulate(ExpressionClassifier* inner,
                  unsigned productions = StandardProductions,
                  bool merge_non_patterns = true) {
    if (merge_non_patterns) MergeNonPatterns(inner);
    // Propagate errors from inner, but don't overwrite already recorded
    // errors.
    unsigned non_arrow_inner_invalid_productions =
        inner->invalid_productions_ & ~ArrowFormalParametersProduction;
    if (non_arrow_inner_invalid_productions == 0) return;
    unsigned non_arrow_productions =
        productions & ~ArrowFormalParametersProduction;
    unsigned errors =
        non_arrow_productions & non_arrow_inner_invalid_productions;
    errors &= ~invalid_productions_;
    if (errors != 0) {
      invalid_productions_ |= errors;
      if (errors & ExpressionProduction)
        expression_error_ = inner->expression_error_;
      if (errors & FormalParameterInitializerProduction)
        formal_parameter_initializer_error_ =
            inner->formal_parameter_initializer_error_;
      if (errors & BindingPatternProduction)
        binding_pattern_error_ = inner->binding_pattern_error_;
      if (errors & AssignmentPatternProduction)
        assignment_pattern_error_ = inner->assignment_pattern_error_;
      if (errors & DistinctFormalParametersProduction)
        duplicate_formal_parameter_error_ =
            inner->duplicate_formal_parameter_error_;
      if (errors & StrictModeFormalParametersProduction)
        strict_mode_formal_parameter_error_ =
            inner->strict_mode_formal_parameter_error_;
      if (errors & StrongModeFormalParametersProduction)
        strong_mode_formal_parameter_error_ =
            inner->strong_mode_formal_parameter_error_;
      if (errors & LetPatternProduction)
        let_pattern_error_ = inner->let_pattern_error_;
      if (errors & CoverInitializedNameProduction)
        cover_initialized_name_error_ = inner->cover_initialized_name_error_;
    }

    // As an exception to the above, the result continues to be a valid arrow
    // formal parameters if the inner expression is a valid binding pattern.
    if (productions & ArrowFormalParametersProduction &&
        is_valid_arrow_formal_parameters()) {
      // Also copy function properties if expecting an arrow function
      // parameter.
      function_properties_ |= inner->function_properties_;

      if (!inner->is_valid_binding_pattern()) {
        invalid_productions_ |= ArrowFormalParametersProduction;
        arrow_formal_parameters_error_ = inner->binding_pattern_error_;
      }
    }
  }

  V8_INLINE int GetNonPatternBegin() const { return non_pattern_begin_; }

  V8_INLINE void Discard() {
    DCHECK_LE(non_pattern_begin_, non_patterns_to_rewrite_->length());
    non_patterns_to_rewrite_->Rewind(non_pattern_begin_);
  }

  V8_INLINE void MergeNonPatterns(ExpressionClassifier* inner) {
    DCHECK_LE(non_pattern_begin_, inner->non_pattern_begin_);
    inner->non_pattern_begin_ = inner->non_patterns_to_rewrite_->length();
  }

 private:
  Zone* zone_;
  ZoneList<typename Traits::Type::Expression>* non_patterns_to_rewrite_;
  int non_pattern_begin_;
  unsigned invalid_productions_;
  unsigned function_properties_;
  Error expression_error_;
  Error formal_parameter_initializer_error_;
  Error binding_pattern_error_;
  Error assignment_pattern_error_;
  Error arrow_formal_parameters_error_;
  Error duplicate_formal_parameter_error_;
  Error strict_mode_formal_parameter_error_;
  Error strong_mode_formal_parameter_error_;
  Error let_pattern_error_;
  Error cover_initialized_name_error_;
  DuplicateFinder* duplicate_finder_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_EXPRESSION_CLASSIFIER_H
