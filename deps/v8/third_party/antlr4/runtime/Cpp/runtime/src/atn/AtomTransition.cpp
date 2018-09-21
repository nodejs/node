/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/Transition.h"
#include "misc/IntervalSet.h"

#include "atn/AtomTransition.h"

using namespace antlr4::misc;
using namespace antlr4::atn;

AtomTransition::AtomTransition(ATNState* target, size_t label)
    : Transition(target), _label(label) {}

Transition::SerializationType AtomTransition::getSerializationType() const {
  return ATOM;
}

IntervalSet AtomTransition::label() const {
  return IntervalSet::of((int)_label);
}

bool AtomTransition::matches(size_t symbol, size_t /*minVocabSymbol*/,
                             size_t /*maxVocabSymbol*/) const {
  return _label == symbol;
}

std::string AtomTransition::toString() const {
  return "ATOM " + Transition::toString() +
         " { label: " + std::to_string(_label) + " }";
}
