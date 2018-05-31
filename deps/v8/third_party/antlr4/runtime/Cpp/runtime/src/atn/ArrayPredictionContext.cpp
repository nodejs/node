/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/SingletonPredictionContext.h"
#include "support/Arrays.h"

#include "atn/ArrayPredictionContext.h"

using namespace antlr4::atn;

ArrayPredictionContext::ArrayPredictionContext(
    Ref<SingletonPredictionContext> const& a)
    : ArrayPredictionContext({a->parent}, {a->returnState}) {}

ArrayPredictionContext::ArrayPredictionContext(
    std::vector<Ref<PredictionContext>> const& parents_,
    std::vector<size_t> const& returnStates)
    : PredictionContext(calculateHashCode(parents_, returnStates)),
      parents(parents_),
      returnStates(returnStates) {
  assert(parents.size() > 0);
  assert(returnStates.size() > 0);
}

ArrayPredictionContext::~ArrayPredictionContext() {}

bool ArrayPredictionContext::isEmpty() const {
  // Since EMPTY_RETURN_STATE can only appear in the last position, we don't
  // need to verify that size == 1.
  return returnStates[0] == EMPTY_RETURN_STATE;
}

size_t ArrayPredictionContext::size() const { return returnStates.size(); }

Ref<PredictionContext> ArrayPredictionContext::getParent(size_t index) const {
  return parents[index];
}

size_t ArrayPredictionContext::getReturnState(size_t index) const {
  return returnStates[index];
}

bool ArrayPredictionContext::operator==(PredictionContext const& o) const {
  if (this == &o) {
    return true;
  }

  const ArrayPredictionContext* other =
      dynamic_cast<const ArrayPredictionContext*>(&o);
  if (other == nullptr || hashCode() != other->hashCode()) {
    return false;  // can't be same if hash is different
  }

  return antlrcpp::Arrays::equals(returnStates, other->returnStates) &&
         antlrcpp::Arrays::equals(parents, other->parents);
}

std::string ArrayPredictionContext::toString() const {
  if (isEmpty()) {
    return "[]";
  }

  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < returnStates.size(); i++) {
    if (i > 0) {
      ss << ", ";
    }
    if (returnStates[i] == EMPTY_RETURN_STATE) {
      ss << "$";
      continue;
    }
    ss << returnStates[i];
    if (parents[i] != nullptr) {
      ss << " " << parents[i]->toString();
    } else {
      ss << "nul";
    }
  }
  ss << "]";
  return ss.str();
}
