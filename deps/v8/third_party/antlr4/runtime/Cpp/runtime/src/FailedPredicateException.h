/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "RecognitionException.h"

namespace antlr4 {

/// A semantic predicate failed during validation.  Validation of predicates
/// occurs when normally parsing the alternative just like matching a token.
/// Disambiguating predicate evaluation occurs when we test a predicate during
/// prediction.
class ANTLR4CPP_PUBLIC FailedPredicateException : public RecognitionException {
 public:
  FailedPredicateException(Parser* recognizer);
  FailedPredicateException(Parser* recognizer, const std::string& predicate);
  FailedPredicateException(Parser* recognizer, const std::string& predicate,
                           const std::string& message);

  virtual size_t getRuleIndex();
  virtual size_t getPredIndex();
  virtual std::string getPredicate();

 private:
  size_t _ruleIndex;
  size_t _predicateIndex;
  std::string _predicate;
};

}  // namespace antlr4
