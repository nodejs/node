/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "SemanticContext.h"
#include "atn/AbstractPredicateTransition.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC PrecedencePredicateTransition final
    : public AbstractPredicateTransition {
 public:
  const int precedence;

  PrecedencePredicateTransition(ATNState* target, int precedence);

  virtual SerializationType getSerializationType() const override;
  virtual bool isEpsilon() const override;
  virtual bool matches(size_t symbol, size_t minVocabSymbol,
                       size_t maxVocabSymbol) const override;
  Ref<SemanticContext::PrecedencePredicate> getPredicate() const;
  virtual std::string toString() const override;
};

}  // namespace atn
}  // namespace antlr4
