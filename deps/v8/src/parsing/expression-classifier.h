// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_EXPRESSION_CLASSIFIER_H_
#define V8_PARSING_EXPRESSION_CLASSIFIER_H_

#include <type_traits>

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

// Expression classifier is split into four parts. The base implementing the
// general expression classifier logic. Two parts that implement the error
// tracking interface, where one is the actual implementation and the other is
// an empty class providing only the interface without logic. The expression
// classifier class then combines the other parts and provides the full
// expression classifier interface by inheriting conditionally, controlled by
// Types::ExpressionClassifierReportErrors, either from the ErrorTracker or the
// EmptyErrorTracker.
//
//                 Base
//                  / \
//                 /   \
//                /     \
//               /       \
//      ErrorTracker    EmptyErrorTracker
//               \       /
//                \     /
//                 \   /
//                  \ /
//           ExpressionClassifier

template <typename Types>
class ExpressionClassifier;

template <typename Types, typename ErrorTracker>
class ExpressionClassifierBase {
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
          arg(nullptr) {}
    V8_INLINE explicit Error(Scanner::Location loc,
                             MessageTemplate::Template msg, ErrorKind k,
                             const char* a = nullptr)
        : location(loc), message(msg), kind(k), arg(a) {}

    Scanner::Location location;
    MessageTemplate::Template message : 28;
    unsigned kind : 4;
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

  explicit ExpressionClassifierBase(typename Types::Base* base,
                                    DuplicateFinder* duplicate_finder = nullptr)
      : base_(base),
        duplicate_finder_(duplicate_finder),
        invalid_productions_(0),
        is_non_simple_parameter_list_(0) {}

  virtual ~ExpressionClassifierBase() = default;

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

  V8_INLINE bool is_simple_parameter_list() const {
    return !is_non_simple_parameter_list_;
  }

  V8_INLINE void RecordNonSimpleParameter() {
    is_non_simple_parameter_list_ = 1;
  }

  V8_INLINE void Accumulate(ExpressionClassifier<Types>* const inner,
                            unsigned productions) {
#ifdef DEBUG
    static_cast<ErrorTracker*>(this)->CheckErrorPositions(inner);
#endif
    // Propagate errors from inner, but don't overwrite already recorded
    // errors.
    unsigned non_arrow_inner_invalid_productions =
        inner->invalid_productions_ & ~ArrowFormalParametersProduction;
    if (non_arrow_inner_invalid_productions) {
      unsigned errors = non_arrow_inner_invalid_productions & productions &
                        ~this->invalid_productions_;
      // The result will continue to be a valid arrow formal parameters if the
      // inner expression is a valid binding pattern.
      bool copy_BP_to_AFP = false;
      if (productions & ArrowFormalParametersProduction &&
          this->is_valid_arrow_formal_parameters()) {
        // Also whether we've seen any non-simple parameters
        // if expecting an arrow function parameter.
        this->is_non_simple_parameter_list_ |=
            inner->is_non_simple_parameter_list_;
        if (!inner->is_valid_binding_pattern()) {
          copy_BP_to_AFP = true;
          this->invalid_productions_ |= ArrowFormalParametersProduction;
        }
      }
      if (errors != 0 || copy_BP_to_AFP) {
        this->invalid_productions_ |= errors;
        static_cast<ErrorTracker*>(this)->AccumulateErrorImpl(
            inner, productions, errors, copy_BP_to_AFP);
      }
    }
    static_cast<ErrorTracker*>(this)->RewindErrors(inner);
  }

 protected:
  typename Types::Base* base_;
  DuplicateFinder* duplicate_finder_;
  unsigned invalid_productions_ : kUnusedError;
  STATIC_ASSERT(kUnusedError <= 15);
  unsigned is_non_simple_parameter_list_ : 1;
};

template <typename Types>
class ExpressionClassifierErrorTracker
    : public ExpressionClassifierBase<Types,
                                      ExpressionClassifierErrorTracker<Types>> {
 public:
  using BaseClassType =
      ExpressionClassifierBase<Types, ExpressionClassifierErrorTracker<Types>>;
  using typename BaseClassType::Error;
  using typename BaseClassType::ErrorKind;
  using TP = typename BaseClassType::TargetProduction;

  ExpressionClassifierErrorTracker(typename Types::Base* base,
                                   DuplicateFinder* duplicate_finder)
      : BaseClassType(base, duplicate_finder),
        reported_errors_(base->impl()->GetReportedErrorList()) {
    reported_errors_begin_ = reported_errors_end_ = reported_errors_->length();
  }

  ~ExpressionClassifierErrorTracker() override { Discard(); }

  V8_INLINE void Discard() {
    if (reported_errors_end_ == reported_errors_->length()) {
      reported_errors_->Rewind(reported_errors_begin_);
      reported_errors_end_ = reported_errors_begin_;
    }
    DCHECK_EQ(reported_errors_begin_, reported_errors_end_);
  }

 protected:
  V8_INLINE const Error& reported_error(ErrorKind kind) const {
    if (this->invalid_productions_ & (1 << kind)) {
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
    reported_errors_->Add(e, this->base_->impl()->zone());
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

 private:
#ifdef DEBUG
  V8_INLINE void CheckErrorPositions(ExpressionClassifier<Types>* const inner) {
    DCHECK_EQ(inner->reported_errors_, this->reported_errors_);
    DCHECK_EQ(inner->reported_errors_begin_, this->reported_errors_end_);
    DCHECK_EQ(inner->reported_errors_end_, this->reported_errors_->length());
  }
#endif

  V8_INLINE void RewindErrors(ExpressionClassifier<Types>* const inner) {
    this->reported_errors_->Rewind(this->reported_errors_end_);
    inner->reported_errors_begin_ = inner->reported_errors_end_ =
        this->reported_errors_end_;
  }

  void AccumulateErrorImpl(ExpressionClassifier<Types>* const inner,
                           unsigned productions, unsigned errors,
                           bool copy_BP_to_AFP) {
    // Traverse the list of errors reported by the inner classifier
    // to copy what's necessary.
    int binding_pattern_index = inner->reported_errors_end_;
    for (int i = inner->reported_errors_begin_; i < inner->reported_errors_end_;
         i++) {
      int k = this->reported_errors_->at(i).kind;
      if (errors & (1 << k)) this->Copy(i);
      // Check if it's a BP error that has to be copied to an AFP error.
      if (k == ErrorKind::kBindingPatternProduction && copy_BP_to_AFP) {
        if (this->reported_errors_end_ <= i) {
          // If the BP error itself has not already been copied,
          // copy it now and change it to an AFP error.
          this->Copy(i);
          this->reported_errors_->at(this->reported_errors_end_ - 1).kind =
              ErrorKind::kArrowFormalParametersProduction;
        } else {
          // Otherwise, if the BP error was already copied, keep its
          // position and wait until the end of the traversal.
          DCHECK_EQ(this->reported_errors_end_, i + 1);
          binding_pattern_index = i;
        }
      }
    }
    // Do we still have to copy the BP error to an AFP error?
    if (binding_pattern_index < inner->reported_errors_end_) {
      // If there's still unused space in the list of the inner
      // classifier, copy it there, otherwise add it to the end
      // of the list.
      if (this->reported_errors_end_ < inner->reported_errors_end_)
        this->Copy(binding_pattern_index);
      else
        Add(this->reported_errors_->at(binding_pattern_index));
      this->reported_errors_->at(this->reported_errors_end_ - 1).kind =
          ErrorKind::kArrowFormalParametersProduction;
    }
  }

 private:
  ZoneList<Error>* reported_errors_;
  // The uint16_t for reported_errors_begin_ and reported_errors_end_ will
  // not be enough in the case of a long series of expressions using nested
  // classifiers, e.g., a long sequence of assignments, as in:
  // literals with spreads, as in:
  // var N=65536; eval("var x;" + "x=".repeat(N) + "42");
  // This should not be a problem, as such things currently fail with a
  // stack overflow while parsing.
  uint16_t reported_errors_begin_;
  uint16_t reported_errors_end_;

  friend BaseClassType;
};

template <typename Types>
class ExpressionClassifierEmptyErrorTracker
    : public ExpressionClassifierBase<
          Types, ExpressionClassifierEmptyErrorTracker<Types>> {
 public:
  using BaseClassType =
      ExpressionClassifierBase<Types,
                               ExpressionClassifierEmptyErrorTracker<Types>>;
  using typename BaseClassType::Error;
  using typename BaseClassType::ErrorKind;
  using TP = typename BaseClassType::TargetProduction;

  ExpressionClassifierEmptyErrorTracker(typename Types::Base* base,
                                        DuplicateFinder* duplicate_finder)
      : BaseClassType(base, duplicate_finder) {}

  V8_INLINE void Discard() {}

 protected:
  V8_INLINE const Error& reported_error(ErrorKind kind) const {
    static Error none;
    return none;
  }

  V8_INLINE void Add(const Error& e) {}

 private:
#ifdef DEBUG
  V8_INLINE void CheckErrorPositions(ExpressionClassifier<Types>* const inner) {
  }
#endif
  V8_INLINE void RewindErrors(ExpressionClassifier<Types>* const inner) {}
  V8_INLINE void AccumulateErrorImpl(ExpressionClassifier<Types>* const inner,
                                     unsigned productions, unsigned errors,
                                     bool copy_BP_to_AFP) {}

  friend BaseClassType;
};

template <typename Types>
class ExpressionClassifier
    : public std::conditional<
          Types::ExpressionClassifierReportErrors,
          ExpressionClassifierErrorTracker<Types>,
          ExpressionClassifierEmptyErrorTracker<Types>>::type {
  static constexpr bool ReportErrors = Types::ExpressionClassifierReportErrors;

 public:
  using BaseClassType = typename std::conditional<
      Types::ExpressionClassifierReportErrors,
      typename ExpressionClassifierErrorTracker<Types>::BaseClassType,
      typename ExpressionClassifierEmptyErrorTracker<Types>::BaseClassType>::
      type;
  using typename BaseClassType::Error;
  using typename BaseClassType::ErrorKind;
  using TP = typename BaseClassType::TargetProduction;

  explicit ExpressionClassifier(typename Types::Base* base,
                                DuplicateFinder* duplicate_finder = nullptr)
      : std::conditional<Types::ExpressionClassifierReportErrors,
                         ExpressionClassifierErrorTracker<Types>,
                         ExpressionClassifierEmptyErrorTracker<Types>>::
            type(base, duplicate_finder),
        previous_(base->classifier_) {
    base->classifier_ = this;
  }

  V8_INLINE ~ExpressionClassifier() override {
    if (this->base_->classifier_ == this) this->base_->classifier_ = previous_;
  }

  V8_INLINE const Error& expression_error() const {
    return this->reported_error(ErrorKind::kExpressionProduction);
  }

  V8_INLINE const Error& formal_parameter_initializer_error() const {
    return this->reported_error(
        ErrorKind::kFormalParameterInitializerProduction);
  }

  V8_INLINE const Error& binding_pattern_error() const {
    return this->reported_error(ErrorKind::kBindingPatternProduction);
  }

  V8_INLINE const Error& assignment_pattern_error() const {
    return this->reported_error(ErrorKind::kAssignmentPatternProduction);
  }

  V8_INLINE const Error& arrow_formal_parameters_error() const {
    return this->reported_error(ErrorKind::kArrowFormalParametersProduction);
  }

  V8_INLINE const Error& duplicate_formal_parameter_error() const {
    return this->reported_error(ErrorKind::kDistinctFormalParametersProduction);
  }

  V8_INLINE const Error& strict_mode_formal_parameter_error() const {
    return this->reported_error(
        ErrorKind::kStrictModeFormalParametersProduction);
  }

  V8_INLINE const Error& let_pattern_error() const {
    return this->reported_error(ErrorKind::kLetPatternProduction);
  }

  V8_INLINE const Error& async_arrow_formal_parameters_error() const {
    return this->reported_error(
        ErrorKind::kAsyncArrowFormalParametersProduction);
  }

  V8_INLINE bool does_error_reporting() { return ReportErrors; }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!this->is_valid_expression()) return;
    this->invalid_productions_ |= TP::ExpressionProduction;
    this->Add(Error(loc, message, ErrorKind::kExpressionProduction, arg));
  }

  void RecordFormalParameterInitializerError(const Scanner::Location& loc,
                                             MessageTemplate::Template message,
                                             const char* arg = nullptr) {
    if (!this->is_valid_formal_parameter_initializer()) return;
    this->invalid_productions_ |= TP::FormalParameterInitializerProduction;
    this->Add(Error(loc, message,
                    ErrorKind::kFormalParameterInitializerProduction, arg));
  }

  void RecordBindingPatternError(const Scanner::Location& loc,
                                 MessageTemplate::Template message,
                                 const char* arg = nullptr) {
    if (!this->is_valid_binding_pattern()) return;
    this->invalid_productions_ |= TP::BindingPatternProduction;
    this->Add(Error(loc, message, ErrorKind::kBindingPatternProduction, arg));
  }

  void RecordAssignmentPatternError(const Scanner::Location& loc,
                                    MessageTemplate::Template message,
                                    const char* arg = nullptr) {
    if (!this->is_valid_assignment_pattern()) return;
    this->invalid_productions_ |= TP::AssignmentPatternProduction;
    this->Add(
        Error(loc, message, ErrorKind::kAssignmentPatternProduction, arg));
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
    if (!this->is_valid_arrow_formal_parameters()) return;
    this->invalid_productions_ |= TP::ArrowFormalParametersProduction;
    this->Add(
        Error(loc, message, ErrorKind::kArrowFormalParametersProduction, arg));
  }

  void RecordAsyncArrowFormalParametersError(const Scanner::Location& loc,
                                             MessageTemplate::Template message,
                                             const char* arg = nullptr) {
    if (!this->is_valid_async_arrow_formal_parameters()) return;
    this->invalid_productions_ |= TP::AsyncArrowFormalParametersProduction;
    this->Add(Error(loc, message,
                    ErrorKind::kAsyncArrowFormalParametersProduction, arg));
  }

  void RecordDuplicateFormalParameterError(const Scanner::Location& loc) {
    if (!this->is_valid_formal_parameter_list_without_duplicates()) return;
    this->invalid_productions_ |= TP::DistinctFormalParametersProduction;
    this->Add(Error(loc, MessageTemplate::kParamDupe,
                    ErrorKind::kDistinctFormalParametersProduction));
  }

  // Record a binding that would be invalid in strict mode.  Confusingly this
  // is not the same as StrictFormalParameterList, which simply forbids
  // duplicate bindings.
  void RecordStrictModeFormalParameterError(const Scanner::Location& loc,
                                            MessageTemplate::Template message,
                                            const char* arg = nullptr) {
    if (!this->is_valid_strict_mode_formal_parameters()) return;
    this->invalid_productions_ |= TP::StrictModeFormalParametersProduction;
    this->Add(Error(loc, message,
                    ErrorKind::kStrictModeFormalParametersProduction, arg));
  }

  void RecordLetPatternError(const Scanner::Location& loc,
                             MessageTemplate::Template message,
                             const char* arg = nullptr) {
    if (!this->is_valid_let_pattern()) return;
    this->invalid_productions_ |= TP::LetPatternProduction;
    this->Add(Error(loc, message, ErrorKind::kLetPatternProduction, arg));
  }

  ExpressionClassifier* previous() const { return previous_; }

 private:
  ExpressionClassifier* previous_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionClassifier);
};

#undef ERROR_CODES

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_EXPRESSION_CLASSIFIER_H_
