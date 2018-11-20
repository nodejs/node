/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "support/Arrays.h"

#include "atn/Transition.h"

using namespace antlr4;
using namespace antlr4::atn;

using namespace antlrcpp;

const std::vector<std::string> Transition::serializationNames = {
    "INVALID", "EPSILON", "RANGE",   "RULE",     "PREDICATE", "ATOM",
    "ACTION",  "SET",     "NOT_SET", "WILDCARD", "PRECEDENCE"};

Transition::Transition(ATNState* target) {
  if (target == nullptr) {
    throw NullPointerException("target cannot be null.");
  }

  this->target = target;
}

Transition::~Transition() {}

bool Transition::isEpsilon() const { return false; }

misc::IntervalSet Transition::label() const {
  return misc::IntervalSet::EMPTY_SET;
}

std::string Transition::toString() const {
  std::stringstream ss;
  ss << "(Transition " << std::hex << this << ", target: " << std::hex << target
     << ')';

  return ss.str();
}
