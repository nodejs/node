/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "misc/IntervalSet.h"

#include "atn/RangeTransition.h"

using namespace antlr4;
using namespace antlr4::atn;

RangeTransition::RangeTransition(ATNState* target, size_t from, size_t to)
    : Transition(target), from(from), to(to) {}

Transition::SerializationType RangeTransition::getSerializationType() const {
  return RANGE;
}

misc::IntervalSet RangeTransition::label() const {
  return misc::IntervalSet::of((int)from, (int)to);
}

bool RangeTransition::matches(size_t symbol, size_t /*minVocabSymbol*/,
                              size_t /*maxVocabSymbol*/) const {
  return symbol >= from && symbol <= to;
}

std::string RangeTransition::toString() const {
  return "RANGE " + Transition::toString() +
         " { from: " + std::to_string(from) + ", to: " + std::to_string(to) +
         " }";
}
