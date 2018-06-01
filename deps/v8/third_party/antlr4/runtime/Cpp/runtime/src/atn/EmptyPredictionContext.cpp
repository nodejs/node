/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/EmptyPredictionContext.h"

using namespace antlr4::atn;

EmptyPredictionContext::EmptyPredictionContext()
    : SingletonPredictionContext(nullptr, EMPTY_RETURN_STATE) {}

bool EmptyPredictionContext::isEmpty() const { return true; }

size_t EmptyPredictionContext::size() const { return 1; }

Ref<PredictionContext> EmptyPredictionContext::getParent(
    size_t /*index*/) const {
  return nullptr;
}

size_t EmptyPredictionContext::getReturnState(size_t /*index*/) const {
  return returnState;
}

bool EmptyPredictionContext::operator==(const PredictionContext& o) const {
  return this == &o;
}

std::string EmptyPredictionContext::toString() const { return "$"; }
