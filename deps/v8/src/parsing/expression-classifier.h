// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_EXPRESSION_CLASSIFIER_H
#define V8_PARSING_EXPRESSION_CLASSIFIER_H

#include "src/messages.h"
#include "src/parsing/scanner.h"

namespace v8 {
namespace internal {

class DuplicateFinder;

#define ERROR_CODES(T)                       \
  T(ExpressionProduction, 0)                 \
  T(FormalParameterInitializerProduction, 1) \
  T(BindingPatternProduction, 2)             \
  T(AssignmentPatternProduction, 3)          \
  T(DistinctFormalParametersProduction, 4)   \
  T(StrictModeFormalParametersProduction, 5) \
  T(ArrowFormalParametersProduction, 6)      \
  T(LetPatternProduction, 7)                 \
  T(AsyncArrowFormalParametersProduction, 8)

// Expression classifiers serve two purposes:
//
// 1) They keep track of error messages that are pending (and other
//    related information), waiting for the parser to decide whether
//    the parsed expression is a pattern or not.
// 2) They keep track of expressions that may need to be rewritten, if
//    the parser decides that they are not patterns.  (A different
//    mechanism implements the rewriting of patterns.)
//
// Expression classifiers are used by the parser in a stack fashion.
// Each new classifier is pushed on top of the stack.  This happens
// automatically by the class's constructor.  While on top of the
// stack, the classifier records pending error messages and tracks the
// pending non-patterns of the expression that is being parsed.
//
// At the end of its life, a classifier is either "accumulated" to the
// one that is below it on the stack, or is "discarded".  The former
// is achieved by calling the method Accumulate.  The latter is
// achieved automatically by the destructor, but it can happen earlier
// by calling the method Discard.  Both actions result in removing the
// classifier from the parser's stack.

template <typename Types>
class ExpressionClassifier {
 public:
  enum ErrorKind : unsigned {
#define DEFINE_ERROR_KIND(NAME, CODE) k##NAME = CODE,
    ERROR_CODES(DEFINE_ERROR_KIND)
#undef DEFINE_ERROR_KIND
    kUnusedError = 15  // Larger than error codes; should fit in 4 bits
  };

  struct Error {
    V8_INLINE Error()
        : location(Scanner::Location::invalid()),
          message(MessageTemplate::kNone),
          kind(kUnusedError),
          type(kSyntaxError),
          arg(nullptr) {}
    V8_INLINE explicit Error(Scanner::Location loc,
                             MessageTemplate::Template msg, ErrorKind k,
                             const char* a = nullptr,
                             ParseErrorType t = kSyntaxError)
        : location(loc), message(msg), kind(k), type(t), arg(a) {}

    Scanner::Location location;
    MessageTemplate::Template message : 26;
    unsigned kind : 4;
    ParseErrorType type : 2;
    const char* arg;
  };

  // clang-format off
  enum TargetProduction : unsigned {
#define DEFINE_PRODUCTION(NAME, CODE) NAME = 1 << CODE,
    ERROR_CODES(DEFINE_PRODUCTION)
#undef DEFINE_PRODUCTION

#define DEFINE_ALL_PRODUCTIONS(NAME, CODE) NAME |
    AllProductions = ERROR_CODES(DEFINE_ALL_PRODUCTIONS) /* | */ 0
#undef DEFINE_ALL_PRODUCTIONS
  };
  // clang-format on

  enum FunctionProperties : unsigned {
    NonSimpleParameter = 1 << 0
  };

  explicit ExpressionClassifier(typename Types::Base* base,
                                DuplicateFinder* duplicate_finder = nullptr)
      : base_(base),
        previous_(base->classifier_),
        zone_(base->impl()->zone()),
        non_patterns_to_rewrite_(base->impl()->GetNonPatternList()),
        reported_errors_(base->impl()->GetReportedErrorList()),
        duplicate_finder_(duplicate_finder),
        invalid_productions_(0),
        function_properties_(0) {
    base->classifier_ = this;
    reported_errors_begin_ = reported_errors_end_ = reported_errors_->length();
    non_pattern_begin_ = non_patterns_to_rewrite_->length();
  }

  V8_INLINE ~ExpressionClassifier() {
    Discard();
    if (base_->classifier_ == this) base_->classifier_ = previous_;
  }

  V8_INLINE bool is_valid(unsigned productions) const {
    return (invalid_productions_ & productions) == 0;
  }

  V8_INLINE DuplicateFinder* duplicate_finder() const {
    return duplicate_finder_;
  }

  V8_INLINE bool is_valid_expression() const {
    return is_valid(ExpressionProduction);
  }

  V8_INLINE bool is_valid_formal_parameter_initializer() const {
    return is_valid(FormalParameterInitializerProduction);
  }

  V8_INLINE bool is_valid_binding_pattern() const {
    return is_valid(BindingPatternProduction);
  }

  V8_INLINE bool is_valid_assignment_pattern() const {
    return is_valid(AssignmentPatternProduction);
  }

  V8_INLINE bool is_valid_arrow_formal_parameters() const {
    return is_valid(ArrowFormalParametersProduction);
  }

  V8_INLINE bool is_valid_formal_parameter_list_without_duplicates() const {
    return is_valid(DistinctFormalParametersProduction);
  }

  // Note: callers should also check
  // is_valid_formal_parameter_list_without_duplicates().
  V8_INLINE bool is_valid_strict_mode_formal_parameters() const {
    return is_valid(StrictModeFormalParametersProduction);
  }

  V8_INLINE bool is_valid_let_pattern() const {
    return is_valid(LetPatternProduction);
  }

  bool is_valid_async_arrow_formal_parameters() const {
    return is_valid(AsyncArrowFormalParametersProduction);
  }

  V8_INLINE const Error& expression_error() const {
    return reported_error(kExpressionProduction);
  }

  V8_INLINE const Error& formal_parameter_initializer_error() const {
    return reported_error(kFormalParameterInitializerProduction);
  }

  V8_INLINE const Error& binding_pattern_error() const {
    return reported_error(kBindingPatternProduction);
  }

  V8_INLINE const Error& assignment_pattern_error() const {
    return reported_error(kAssignmentPatternProduction);
  }

  V8_INLINE const Error& arrow_formal_parameters_error() const {
    return reported_error(kArrowFormalParametersProduction);
  }

  V8_INLINE const Error& duplicate_formal_parameter_error() const {
    return reported_error(kDistinctFormalParametersProduction);
  }

  V8_INLINE const Error& strict_mode_formal_parameter_error() const {
    return reported_error(kStrictModeFormalParametersProduction);
  }

  V8_INLINE const Error& let_pattern_error() const {
    return reported_error(kLetPatternProduction);
  }

  V8_INLINE const Error& async_arrow_formal_parameters_error() const {
    return reported_error(kAsyncArrowFormalParametersProduction);
  }

  V8_INLINE bool is_simple_parameter_list() const {
    return !(function_properties_ & NonSimpleParameter);
  }

  V8_INLINE void RecordNonSimpleParameter() {
    function_properties_ |= NonSimpleParameter;
  }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!is_valid_expression()) return;
    invalid_productions_ |= ExpressionProduction;
    Add(Error(loc, message, kExpressionProduction, arg));
  }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             ParseErrorType type, const char* arg = nullptr) {
    if (!is_valid_expression()) return;
    invalid_productions_ |= ExpressionProduction;
    Add(Error(loc, message, kExpressionProduction, arg, type));
  }

  void RecordFormalParameterInitializerError(const Scanner::Location& loc,
                                             MessageTemplate::Template message,
                                             const char* arg = nullptr) {
    if (!is_valid_formal_parameter_initializer()) return;
    invalid_productions_ |= FormalParameterInitializerProduction;
    Add(Error(loc, message, kFormalParameterInitializerProduction, arg));
  }

  void RecordBindingPatternError(const Scanner::Location& loc,
                                 MessageTemplate::Template message,
                                 const char* arg = nullptr) {
    if (!is_valid_binding_pattern()) return;
    invalid_productions_ |= BindingPatternProduction;
    Add(Error(loc, message, kBindingPatternProduction, arg));
  }

  void RecordAssignmentPatternError(const Scanner::Location& loc,
                                    MessageTemplate::Template message,
                                    const char* arg = nullptr) {
    if (!is_valid_assignment_pattern()) return;
    invalid_productions_ |= AssignmentPatternProduction;
    Add(Error(loc, message, kAssignmentPatternProduction, arg));
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
    Add(Error(loc, message, kArrowFormalParametersProduction, arg));
  }

  void RecordAsyncArrowFormalParametersError(const Scanner::Location& loc,
                                             MessageTemplate::Template message,
                                             const char* arg = nullptr) {
    if (!is_valid_async_arrow_formal_parameters()) return;
    invalid_productions_ |= AsyncArrowFormalParametersProduction;
    Add(Error(loc, message, kAsyncArrowFormalParametersProduction, arg));
  }

  void RecordDuplicateFormalParameterError(const Scanner::Location& loc) {
    if (!is_valid_formal_parameter_list_without_duplicates()) return;
    invalid_productions_ |= DistinctFormalParametersProduction;
    Add(Error(loc, MessageTemplate::kParamDupe,
              kDistinctFormalParametersProduction));
  }

  // Record a binding that would be invalid in strict mode.  Confusingly this
  // is not the same as StrictFormalParameterList, which simply forbids
  // duplicate bindings.
  void RecordStrictModeFormalParameterError(const Scanner::Location& loc,
                                            MessageTemplate::Template message,
                                            const char* arg = nullptr) {
    if (!is_valid_strict_mode_formal_parameters()) return;
    invalid_productions_ |= StrictModeFormalParametersProduction;
    Add(Error(loc, message, kStrictModeFormalParametersProduction, arg));
  }

  void RecordLetPatternError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!is_valid_let_pattern()) return;
    invalid_productions_ |= LetPatternProduction;
    Add(Error(loc, message, kLetPatternProduction, arg));
  }

  void Accumulate(ExpressionClassifier* inner, unsigned productions,
                  bool merge_non_patterns = true) {
    DCHECK_EQ(inner->reported_errors_, reported_errors_);
    DCHECK_EQ(inner->reported_errors_begin_, reported_errors_end_);
    DCHECK_EQ(inner->reported_errors_end_, reported_errors_->length());
    DCHECK_EQ(inner->non_patterns_to_rewrite_, non_patterns_to_rewrite_);
    DCHECK_LE(non_pattern_begin_, inner->non_pattern_begin_);
    DCHECK_LE(inner->non_pattern_begin_, non_patterns_to_rewrite_->length());
    // Merge non-patterns from the inner classifier, or discard them.
    if (merge_non_patterns)
      inner->non_pattern_begin_ = non_patterns_to_rewrite_->length();
    else
      non_patterns_to_rewrite_->Rewind(inner->non_pattern_begin_);
    // Propagate errors from inner, but don't overwrite already recorded
    // errors.
    unsigned non_arrow_inner_invalid_productions =
        inner->invalid_productions_ & ~ArrowFormalParametersProduction;
    if (non_arrow_inner_invalid_productions) {
      unsigned errors = non_arrow_inner_invalid_productions & productions &
                        ~invalid_productions_;
      // The result will continue to be a valid arrow formal parameters if the
      // inner expression is a valid binding pattern.
      bool copy_BP_to_AFP = false;
      if (productions & ArrowFormalParametersProduction &&
          is_valid_arrow_formal_parameters()) {
        // Also copy function properties if expecting an arrow function
        // parameter.
        function_properties_ |= inner->function_properties_;
        if (!inner->is_valid_binding_pattern()) {
          copy_BP_to_AFP = true;
          invalid_productions_ |= ArrowFormalParametersProduction;
        }
      }
      // Traverse the list of errors reported by the inner classifier
      // to copy what's necessary.
      if (errors != 0 || copy_BP_to_AFP) {
        invalid_productions_ |= errors;
        int binding_pattern_index = inner->reported_errors_end_;
        for (int i = inner->reported_errors_begin_;
             i < inner->reported_errors_end_; i++) {
          int k = reported_errors_->at(i).kind;
          if (errors & (1 << k)) Copy(i);
          // Check if it's a BP error that has to be copied to an AFP error.
          if (k == kBindingPatternProduction && copy_BP_to_AFP) {
            if (reported_errors_end_ <= i) {
              // If the BP error itself has not already been copied,
              // copy it now and change it to an AFP error.
              Copy(i);
              reported_errors_->at(reported_errors_end_-1).kind =
                  kArrowFormalParametersProduction;
            } else {
              // Otherwise, if the BP error was already copied, keep its
              // position and wait until the end of the traversal.
              DCHECK_EQ(reported_errors_end_, i+1);
              binding_pattern_index = i;
            }
          }
        }
        // Do we still have to copy the BP error to an AFP error?
        if (binding_pattern_index < inner->reported_errors_end_) {
          // If there's still unused space in the list of the inner
          // classifier, copy it there, otherwise add it to the end
          // of the list.
          if (reported_errors_end_ < inner->reported_errors_end_)
            Copy(binding_pattern_index);
          else
            Add(reported_errors_->at(binding_pattern_index));
          reported_errors_->at(reported_errors_end_-1).kind =
              kArrowFormalParametersProduction;
        }
      }
    }
    reported_errors_->Rewind(reported_errors_end_);
    inner->reported_errors_begin_ = inner->reported_errors_end_ =
        reported_errors_end_;
  }

  V8_INLINE int GetNonPatternBegin() const { return non_pattern_begin_; }

  V8_INLINE void Discard() {
    if (reported_errors_end_ == reported_errors_->length()) {
      reported_errors_->Rewind(reported_errors_begin_);
      reported_errors_end_ = reported_errors_begin_;
    }
    DCHECK_EQ(reported_errors_begin_, reported_errors_end_);
    DCHECK_LE(non_pattern_begin_, non_patterns_to_rewrite_->length());
    non_patterns_to_rewrite_->Rewind(non_pattern_begin_);
  }

  ExpressionClassifier* previous() const { return previous_; }

 private:
  V8_INLINE const Error& reported_error(ErrorKind kind) const {
    if (invalid_productions_ & (1 << kind)) {
      for (int i = reported_errors_begin_; i < reported_errors_end_; i++) {
        if (reported_errors_->at(i).kind == kind)
          return reported_errors_->at(i);
      }
      UNREACHABLE();
    }
    // We should only be looking for an error when we know that one has
    // been reported.  But we're not...  So this is to make sure we have
    // the same behaviour.
    UNREACHABLE();

    // Make MSVC happy by returning an error from this inaccessible path.
    static Error none;
    return none;
  }

  // Adds e to the end of the list of reported errors for this classifier.
  // It is expected that this classifier is the last one in the stack.
  V8_INLINE void Add(const Error& e) {
    DCHECK_EQ(reported_errors_end_, reported_errors_->length());
    reported_errors_->Add(e, zone_);
    reported_errors_end_++;
  }

  // Copies the error at position i of the list of reported errors, so that
  // it becomes the last error reported for this classifier.  Position i
  // could be either after the existing errors of this classifier (i.e.,
  // in an inner classifier) or it could be an existing error (in case a
  // copy is needed).
  V8_INLINE void Copy(int i) {
    DCHECK_LT(i, reported_errors_->length());
    if (reported_errors_end_ != i)
      reported_errors_->at(reported_errors_end_) = reported_errors_->at(i);
    reported_errors_end_++;
  }

  typename Types::Base* base_;
  ExpressionClassifier* previous_;
  Zone* zone_;
  ZoneList<typename Types::Expression>* non_patterns_to_rewrite_;
  ZoneList<Error>* reported_errors_;
  DuplicateFinder* duplicate_finder_;
  // The uint16_t for non_pattern_begin_ will not be enough in the case,
  // e.g., of an array literal containing more than 64K inner array
  // literals with spreads, as in:
  // var N=65536; eval("var x=[];" + "[" + "[...x],".repeat(N) + "].length");
  // An implementation limit error in ParserBase::AddNonPatternForRewriting
  // will be triggered in this case.
  uint16_t non_pattern_begin_;
  unsigned invalid_productions_ : 14;
  unsigned function_properties_ : 2;
  // The uint16_t for reported_errors_begin_ and reported_errors_end_ will
  // not be enough in the case of a long series of expressions using nested
  // classifiers, e.g., a long sequence of assignments, as in:
  // literals with spreads, as in:
  // var N=65536; eval("var x;" + "x=".repeat(N) + "42");
  // This should not be a problem, as such things currently fail with a
  // stack overflow while parsing.
  uint16_t reported_errors_begin_;
  uint16_t reported_errors_end_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionClassifier);
};


#undef ERROR_CODES


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_EXPRESSION_CLASSIFIER_H
