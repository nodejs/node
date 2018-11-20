/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "misc/MurmurHash.h"
#include "support/Arrays.h"
#include "support/CPPUtils.h"

#include "SemanticContext.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlrcpp;

//------------------ Predicate
//-----------------------------------------------------------------------------------------

SemanticContext::Predicate::Predicate()
    : Predicate(INVALID_INDEX, INVALID_INDEX, false) {}

SemanticContext::Predicate::Predicate(size_t ruleIndex, size_t predIndex,
                                      bool isCtxDependent)
    : ruleIndex(ruleIndex),
      predIndex(predIndex),
      isCtxDependent(isCtxDependent) {}

bool SemanticContext::Predicate::eval(Recognizer* parser,
                                      RuleContext* parserCallStack) {
  RuleContext* localctx = nullptr;
  if (isCtxDependent) localctx = parserCallStack;
  return parser->sempred(localctx, ruleIndex, predIndex);
}

size_t SemanticContext::Predicate::hashCode() const {
  size_t hashCode = misc::MurmurHash::initialize();
  hashCode = misc::MurmurHash::update(hashCode, ruleIndex);
  hashCode = misc::MurmurHash::update(hashCode, predIndex);
  hashCode = misc::MurmurHash::update(hashCode, isCtxDependent ? 1 : 0);
  hashCode = misc::MurmurHash::finish(hashCode, 3);
  return hashCode;
}

bool SemanticContext::Predicate::operator==(
    const SemanticContext& other) const {
  if (this == &other) return true;

  const Predicate* p = dynamic_cast<const Predicate*>(&other);
  if (p == nullptr) return false;

  return ruleIndex == p->ruleIndex && predIndex == p->predIndex &&
         isCtxDependent == p->isCtxDependent;
}

std::string SemanticContext::Predicate::toString() const {
  return std::string("{") + std::to_string(ruleIndex) + std::string(":") +
         std::to_string(predIndex) + std::string("}?");
}

//------------------ PrecedencePredicate
//-------------------------------------------------------------------------------

SemanticContext::PrecedencePredicate::PrecedencePredicate() : precedence(0) {}

SemanticContext::PrecedencePredicate::PrecedencePredicate(int precedence)
    : precedence(precedence) {}

bool SemanticContext::PrecedencePredicate::eval(Recognizer* parser,
                                                RuleContext* parserCallStack) {
  return parser->precpred(parserCallStack, precedence);
}

Ref<SemanticContext> SemanticContext::PrecedencePredicate::evalPrecedence(
    Recognizer* parser, RuleContext* parserCallStack,
    const Ref<SemanticContext>& this_ref) {
  if (parser->precpred(parserCallStack, precedence)) {
    return SemanticContext::NONE;
  } else {
    return nullptr;
  }
}

int SemanticContext::PrecedencePredicate::compareTo(PrecedencePredicate* o) {
  return precedence - o->precedence;
}

size_t SemanticContext::PrecedencePredicate::hashCode() const {
  size_t hashCode = 1;
  hashCode = 31 * hashCode + static_cast<size_t>(precedence);
  return hashCode;
}

bool SemanticContext::PrecedencePredicate::operator==(
    const SemanticContext& other) const {
  if (this == &other) return true;

  const PrecedencePredicate* predicate =
      dynamic_cast<const PrecedencePredicate*>(&other);
  if (predicate == nullptr) return false;

  return precedence == predicate->precedence;
}

std::string SemanticContext::PrecedencePredicate::toString() const {
  return "{" + std::to_string(precedence) + ">=prec}?";
}

//------------------ AND
//-----------------------------------------------------------------------------------------------

SemanticContext::AND::AND(Ref<SemanticContext> const& a,
                          Ref<SemanticContext> const& b) {
  Set operands;

  if (is<AND>(a)) {
    for (auto operand : std::dynamic_pointer_cast<AND>(a)->opnds) {
      operands.insert(operand);
    }
  } else {
    operands.insert(a);
  }

  if (is<AND>(b)) {
    for (auto operand : std::dynamic_pointer_cast<AND>(b)->opnds) {
      operands.insert(operand);
    }
  } else {
    operands.insert(b);
  }

  std::vector<Ref<PrecedencePredicate>> precedencePredicates =
      filterPrecedencePredicates(operands);

  if (!precedencePredicates.empty()) {
    // interested in the transition with the lowest precedence
    auto predicate = [](Ref<PrecedencePredicate> const& a,
                        Ref<PrecedencePredicate> const& b) {
      return a->precedence < b->precedence;
    };

    auto reduced = std::min_element(precedencePredicates.begin(),
                                    precedencePredicates.end(), predicate);
    operands.insert(*reduced);
  }

  std::copy(operands.begin(), operands.end(), std::back_inserter(opnds));
}

std::vector<Ref<SemanticContext>> SemanticContext::AND::getOperands() const {
  return opnds;
}

bool SemanticContext::AND::operator==(const SemanticContext& other) const {
  if (this == &other) return true;

  const AND* context = dynamic_cast<const AND*>(&other);
  if (context == nullptr) return false;

  return Arrays::equals(opnds, context->opnds);
}

size_t SemanticContext::AND::hashCode() const {
  return misc::MurmurHash::hashCode(opnds, typeid(AND).hash_code());
}

bool SemanticContext::AND::eval(Recognizer* parser,
                                RuleContext* parserCallStack) {
  for (auto opnd : opnds) {
    if (!opnd->eval(parser, parserCallStack)) {
      return false;
    }
  }
  return true;
}

Ref<SemanticContext> SemanticContext::AND::evalPrecedence(
    Recognizer* parser, RuleContext* parserCallStack,
    const Ref<SemanticContext>& this_ref) {
  bool differs = false;
  std::vector<Ref<SemanticContext>> operands;
  for (auto context : opnds) {
    Ref<SemanticContext> evaluated =
        context->evalPrecedence(parser, parserCallStack, context);
    differs |= (evaluated != context);
    if (evaluated == nullptr) {
      // The AND context is false if any element is false.
      return nullptr;
    } else if (evaluated != NONE) {
      // Reduce the result by skipping true elements.
      operands.push_back(evaluated);
    }
  }

  if (!differs) {
    return this_ref;
  }

  if (operands.empty()) {
    // All elements were true, so the AND context is true.
    return NONE;
  }

  Ref<SemanticContext> result = operands[0];
  for (size_t i = 1; i < operands.size(); ++i) {
    result = SemanticContext::And(result, operands[i]);
  }

  return result;
}

std::string SemanticContext::AND::toString() const {
  std::string tmp;
  for (auto var : opnds) {
    tmp += var->toString() + " && ";
  }
  return tmp;
}

//------------------ OR
//------------------------------------------------------------------------------------------------

SemanticContext::OR::OR(Ref<SemanticContext> const& a,
                        Ref<SemanticContext> const& b) {
  Set operands;

  if (is<OR>(a)) {
    for (auto operand : std::dynamic_pointer_cast<OR>(a)->opnds) {
      operands.insert(operand);
    }
  } else {
    operands.insert(a);
  }

  if (is<OR>(b)) {
    for (auto operand : std::dynamic_pointer_cast<OR>(b)->opnds) {
      operands.insert(operand);
    }
  } else {
    operands.insert(b);
  }

  std::vector<Ref<PrecedencePredicate>> precedencePredicates =
      filterPrecedencePredicates(operands);
  if (!precedencePredicates.empty()) {
    // interested in the transition with the highest precedence
    auto predicate = [](Ref<PrecedencePredicate> const& a,
                        Ref<PrecedencePredicate> const& b) {
      return a->precedence < b->precedence;
    };
    auto reduced = std::max_element(precedencePredicates.begin(),
                                    precedencePredicates.end(), predicate);
    operands.insert(*reduced);
  }

  std::copy(operands.begin(), operands.end(), std::back_inserter(opnds));
}

std::vector<Ref<SemanticContext>> SemanticContext::OR::getOperands() const {
  return opnds;
}

bool SemanticContext::OR::operator==(const SemanticContext& other) const {
  if (this == &other) return true;

  const OR* context = dynamic_cast<const OR*>(&other);
  if (context == nullptr) return false;

  return Arrays::equals(opnds, context->opnds);
}

size_t SemanticContext::OR::hashCode() const {
  return misc::MurmurHash::hashCode(opnds, typeid(OR).hash_code());
}

bool SemanticContext::OR::eval(Recognizer* parser,
                               RuleContext* parserCallStack) {
  for (auto opnd : opnds) {
    if (opnd->eval(parser, parserCallStack)) {
      return true;
    }
  }
  return false;
}

Ref<SemanticContext> SemanticContext::OR::evalPrecedence(
    Recognizer* parser, RuleContext* parserCallStack,
    const Ref<SemanticContext>& this_ref) {
  bool differs = false;
  std::vector<Ref<SemanticContext>> operands;
  for (auto context : opnds) {
    Ref<SemanticContext> evaluated =
        context->evalPrecedence(parser, parserCallStack, context);
    differs |= (evaluated != context);
    if (evaluated == NONE) {
      // The OR context is true if any element is true.
      return NONE;
    } else if (evaluated != nullptr) {
      // Reduce the result by skipping false elements.
      operands.push_back(evaluated);
    }
  }

  if (!differs) {
    return this_ref;
  }

  if (operands.empty()) {
    // All elements were false, so the OR context is false.
    return nullptr;
  }

  Ref<SemanticContext> result = operands[0];
  for (size_t i = 1; i < operands.size(); ++i) {
    result = SemanticContext::Or(result, operands[i]);
  }

  return result;
}

std::string SemanticContext::OR::toString() const {
  std::string tmp;
  for (auto var : opnds) {
    tmp += var->toString() + " || ";
  }
  return tmp;
}

//------------------ SemanticContext
//-----------------------------------------------------------------------------------

const Ref<SemanticContext> SemanticContext::NONE =
    std::make_shared<Predicate>(INVALID_INDEX, INVALID_INDEX, false);

SemanticContext::~SemanticContext() {}

bool SemanticContext::operator!=(const SemanticContext& other) const {
  return !(*this == other);
}

Ref<SemanticContext> SemanticContext::evalPrecedence(
    Recognizer* /*parser*/, RuleContext* /*parserCallStack*/,
    const Ref<SemanticContext>& this_ref) {
  return this_ref;
}

Ref<SemanticContext> SemanticContext::And(Ref<SemanticContext> const& a,
                                          Ref<SemanticContext> const& b) {
  if (!a || a == NONE) {
    return b;
  }

  if (!b || b == NONE) {
    return a;
  }

  Ref<AND> result = std::make_shared<AND>(a, b);
  if (result->opnds.size() == 1) {
    return result->opnds[0];
  }

  return result;
}

Ref<SemanticContext> SemanticContext::Or(Ref<SemanticContext> const& a,
                                         Ref<SemanticContext> const& b) {
  if (!a) {
    return b;
  }
  if (!b) {
    return a;
  }

  if (a == NONE || b == NONE) {
    return NONE;
  }

  Ref<OR> result = std::make_shared<OR>(a, b);
  if (result->opnds.size() == 1) {
    return result->opnds[0];
  }

  return result;
}

std::vector<Ref<SemanticContext::PrecedencePredicate>>
SemanticContext::filterPrecedencePredicates(const Set& collection) {
  std::vector<Ref<SemanticContext::PrecedencePredicate>> result;
  for (auto context : collection) {
    if (antlrcpp::is<PrecedencePredicate>(context)) {
      result.push_back(std::dynamic_pointer_cast<PrecedencePredicate>(context));
    }
  }

  return result;
}

//------------------ Operator
//-----------------------------------------------------------------------------------------

SemanticContext::Operator::~Operator() {}
