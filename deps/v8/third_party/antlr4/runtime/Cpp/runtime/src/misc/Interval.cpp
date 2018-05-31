/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "misc/Interval.h"

using namespace antlr4::misc;

size_t antlr4::misc::numericToSymbol(ssize_t v) {
  return static_cast<size_t>(v);
}

ssize_t antlr4::misc::symbolToNumeric(size_t v) {
  return static_cast<ssize_t>(v);
}

Interval const Interval::INVALID;

Interval::Interval()
    : Interval(static_cast<ssize_t>(-1),
               -2) {  // Need an explicit cast here for VS.
}

Interval::Interval(size_t a_, size_t b_)
    : Interval(symbolToNumeric(a_), symbolToNumeric(b_)) {}

Interval::Interval(ssize_t a_, ssize_t b_) : a(a_), b(b_) {}

size_t Interval::length() const {
  if (b < a) {
    return 0;
  }
  return size_t(b - a + 1);
}

bool Interval::operator==(const Interval& other) const {
  return a == other.a && b == other.b;
}

size_t Interval::hashCode() const {
  size_t hash = 23;
  hash = hash * 31 + static_cast<size_t>(a);
  hash = hash * 31 + static_cast<size_t>(b);
  return hash;
}

bool Interval::startsBeforeDisjoint(const Interval& other) const {
  return a < other.a && b < other.a;
}

bool Interval::startsBeforeNonDisjoint(const Interval& other) const {
  return a <= other.a && b >= other.a;
}

bool Interval::startsAfter(const Interval& other) const { return a > other.a; }

bool Interval::startsAfterDisjoint(const Interval& other) const {
  return a > other.b;
}

bool Interval::startsAfterNonDisjoint(const Interval& other) const {
  return a > other.a && a <= other.b;  // b >= other.b implied
}

bool Interval::disjoint(const Interval& other) const {
  return startsBeforeDisjoint(other) || startsAfterDisjoint(other);
}

bool Interval::adjacent(const Interval& other) const {
  return a == other.b + 1 || b == other.a - 1;
}

bool Interval::properlyContains(const Interval& other) const {
  return other.a >= a && other.b <= b;
}

Interval Interval::Union(const Interval& other) const {
  return Interval(std::min(a, other.a), std::max(b, other.b));
}

Interval Interval::intersection(const Interval& other) const {
  return Interval(std::max(a, other.a), std::min(b, other.b));
}

std::string Interval::toString() const {
  return std::to_string(a) + ".." + std::to_string(b);
}
