/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "Lexer.h"
#include "Vocabulary.h"
#include "misc/MurmurHash.h"

#include "misc/IntervalSet.h"

using namespace antlr4;
using namespace antlr4::misc;

IntervalSet const IntervalSet::COMPLETE_CHAR_SET =
    IntervalSet::of(Lexer::MIN_CHAR_VALUE, Lexer::MAX_CHAR_VALUE);

IntervalSet const IntervalSet::EMPTY_SET;

IntervalSet::IntervalSet() : _intervals() {}

IntervalSet::IntervalSet(const IntervalSet& set) : IntervalSet() {
  _intervals = set._intervals;
}

IntervalSet::IntervalSet(IntervalSet&& set)
    : IntervalSet(std::move(set._intervals)) {}

IntervalSet::IntervalSet(std::vector<Interval>&& intervals)
    : _intervals(std::move(intervals)) {}

IntervalSet& IntervalSet::operator=(const IntervalSet& other) {
  _intervals = other._intervals;
  return *this;
}

IntervalSet& IntervalSet::operator=(IntervalSet&& other) {
  _intervals = move(other._intervals);
  return *this;
}

IntervalSet IntervalSet::of(ssize_t a) { return IntervalSet({Interval(a, a)}); }

IntervalSet IntervalSet::of(ssize_t a, ssize_t b) {
  return IntervalSet({Interval(a, b)});
}

void IntervalSet::clear() { _intervals.clear(); }

void IntervalSet::add(ssize_t el) { add(el, el); }

void IntervalSet::add(ssize_t a, ssize_t b) { add(Interval(a, b)); }

void IntervalSet::add(const Interval& addition) {
  if (addition.b < addition.a) {
    return;
  }

  // find position in list
  for (auto iterator = _intervals.begin(); iterator != _intervals.end();
       ++iterator) {
    Interval r = *iterator;
    if (addition == r) {
      return;
    }

    if (addition.adjacent(r) || !addition.disjoint(r)) {
      // next to each other, make a single larger interval
      Interval bigger = addition.Union(r);
      *iterator = bigger;

      // make sure we didn't just create an interval that
      // should be merged with next interval in list
      while (iterator + 1 != _intervals.end()) {
        Interval next = *++iterator;
        if (!bigger.adjacent(next) && bigger.disjoint(next)) {
          break;
        }

        // if we bump up against or overlap next, merge
        iterator = _intervals.erase(iterator);  // remove this one
        --iterator;                      // move backwards to what we just set
        *iterator = bigger.Union(next);  // set to 3 merged ones
        // ml: no need to advance iterator, we do that in the next round anyway.
        // ++iterator; // first call to next after previous duplicates the
        // result
      }
      return;
    }

    if (addition.startsBeforeDisjoint(r)) {
      // insert before r
      //--iterator;
      _intervals.insert(iterator, addition);
      return;
    }

    // if disjoint and after r, a future iteration will handle it
  }

  // ok, must be after last interval (and disjoint from last interval)
  // just add it
  _intervals.push_back(addition);
}

IntervalSet IntervalSet::Or(const std::vector<IntervalSet>& sets) {
  IntervalSet result;
  for (auto& s : sets) {
    result.addAll(s);
  }
  return result;
}

IntervalSet& IntervalSet::addAll(const IntervalSet& set) {
  // walk set and add each interval
  for (auto const& interval : set._intervals) {
    add(interval);
  }
  return *this;
}

IntervalSet IntervalSet::complement(ssize_t minElement,
                                    ssize_t maxElement) const {
  return complement(IntervalSet::of(minElement, maxElement));
}

IntervalSet IntervalSet::complement(const IntervalSet& vocabulary) const {
  return vocabulary.subtract(*this);
}

IntervalSet IntervalSet::subtract(const IntervalSet& other) const {
  return subtract(*this, other);
}

IntervalSet IntervalSet::subtract(const IntervalSet& left,
                                  const IntervalSet& right) {
  if (left.isEmpty()) {
    return IntervalSet();
  }

  if (right.isEmpty()) {
    // right set has no elements; just return the copy of the current set
    return left;
  }

  IntervalSet result(left);
  size_t resultI = 0;
  size_t rightI = 0;
  while (resultI < result._intervals.size() &&
         rightI < right._intervals.size()) {
    Interval& resultInterval = result._intervals[resultI];
    const Interval& rightInterval = right._intervals[rightI];

    // operation: (resultInterval - rightInterval) and update indexes

    if (rightInterval.b < resultInterval.a) {
      rightI++;
      continue;
    }

    if (rightInterval.a > resultInterval.b) {
      resultI++;
      continue;
    }

    Interval beforeCurrent;
    Interval afterCurrent;
    if (rightInterval.a > resultInterval.a) {
      beforeCurrent = Interval(resultInterval.a, rightInterval.a - 1);
    }

    if (rightInterval.b < resultInterval.b) {
      afterCurrent = Interval(rightInterval.b + 1, resultInterval.b);
    }

    if (beforeCurrent.a > -1) {  // -1 is the default value
      if (afterCurrent.a > -1) {
        // split the current interval into two
        result._intervals[resultI] = beforeCurrent;
        result._intervals.insert(result._intervals.begin() + resultI + 1,
                                 afterCurrent);
        resultI++;
        rightI++;
      } else {
        // replace the current interval
        result._intervals[resultI] = beforeCurrent;
        resultI++;
      }
    } else {
      if (afterCurrent.a > -1) {
        // replace the current interval
        result._intervals[resultI] = afterCurrent;
        rightI++;
      } else {
        // remove the current interval (thus no need to increment resultI)
        result._intervals.erase(result._intervals.begin() + resultI);
      }
    }
  }

  // If rightI reached right.intervals.size(), no more intervals to subtract
  // from result. If resultI reached result.intervals.size(), we would be
  // subtracting from an empty set. Either way, we are done.
  return result;
}

IntervalSet IntervalSet::Or(const IntervalSet& a) const {
  IntervalSet result;
  result.addAll(*this);
  result.addAll(a);
  return result;
}

IntervalSet IntervalSet::And(const IntervalSet& other) const {
  IntervalSet intersection;
  size_t i = 0;
  size_t j = 0;

  // iterate down both interval lists looking for nondisjoint intervals
  while (i < _intervals.size() && j < other._intervals.size()) {
    Interval mine = _intervals[i];
    Interval theirs = other._intervals[j];

    if (mine.startsBeforeDisjoint(theirs)) {
      // move this iterator looking for interval that might overlap
      i++;
    } else if (theirs.startsBeforeDisjoint(mine)) {
      // move other iterator looking for interval that might overlap
      j++;
    } else if (mine.properlyContains(theirs)) {
      // overlap, add intersection, get next theirs
      intersection.add(mine.intersection(theirs));
      j++;
    } else if (theirs.properlyContains(mine)) {
      // overlap, add intersection, get next mine
      intersection.add(mine.intersection(theirs));
      i++;
    } else if (!mine.disjoint(theirs)) {
      // overlap, add intersection
      intersection.add(mine.intersection(theirs));

      // Move the iterator of lower range [a..b], but not
      // the upper range as it may contain elements that will collide
      // with the next iterator. So, if mine=[0..115] and
      // theirs=[115..200], then intersection is 115 and move mine
      // but not theirs as theirs may collide with the next range
      // in thisIter.
      // move both iterators to next ranges
      if (mine.startsAfterNonDisjoint(theirs)) {
        j++;
      } else if (theirs.startsAfterNonDisjoint(mine)) {
        i++;
      }
    }
  }

  return intersection;
}

bool IntervalSet::contains(size_t el) const {
  return contains(symbolToNumeric(el));
}

bool IntervalSet::contains(ssize_t el) const {
  if (_intervals.empty()) return false;

  if (el < _intervals[0]
               .a)  // list is sorted and el is before first interval; not here
    return false;

  for (auto& interval : _intervals) {
    if (el >= interval.a && el <= interval.b) {
      return true;  // found in this interval
    }
  }
  return false;
}

bool IntervalSet::isEmpty() const { return _intervals.empty(); }

ssize_t IntervalSet::getSingleElement() const {
  if (_intervals.size() == 1) {
    if (_intervals[0].a == _intervals[0].b) {
      return _intervals[0].a;
    }
  }

  return Token::INVALID_TYPE;  // XXX: this value is 0, but 0 is a valid
                               // interval range, how can that work?
}

ssize_t IntervalSet::getMaxElement() const {
  if (_intervals.empty()) {
    return Token::INVALID_TYPE;
  }

  return _intervals.back().b;
}

ssize_t IntervalSet::getMinElement() const {
  if (_intervals.empty()) {
    return Token::INVALID_TYPE;
  }

  return _intervals[0].a;
}

std::vector<Interval> const& IntervalSet::getIntervals() const {
  return _intervals;
}

size_t IntervalSet::hashCode() const {
  size_t hash = MurmurHash::initialize();
  for (auto& interval : _intervals) {
    hash = MurmurHash::update(hash, interval.a);
    hash = MurmurHash::update(hash, interval.b);
  }

  return MurmurHash::finish(hash, _intervals.size() * 2);
}

bool IntervalSet::operator==(const IntervalSet& other) const {
  if (_intervals.empty() && other._intervals.empty()) return true;

  if (_intervals.size() != other._intervals.size()) return false;

  return std::equal(_intervals.begin(), _intervals.end(),
                    other._intervals.begin());
}

std::string IntervalSet::toString() const { return toString(false); }

std::string IntervalSet::toString(bool elemAreChar) const {
  if (_intervals.empty()) {
    return "{}";
  }

  std::stringstream ss;
  size_t effectiveSize = size();
  if (effectiveSize > 1) {
    ss << "{";
  }

  bool firstEntry = true;
  for (auto& interval : _intervals) {
    if (!firstEntry) ss << ", ";
    firstEntry = false;

    ssize_t a = interval.a;
    ssize_t b = interval.b;
    if (a == b) {
      if (a == -1) {
        ss << "<EOF>";
      } else if (elemAreChar) {
        ss << "'" << static_cast<char>(a) << "'";
      } else {
        ss << a;
      }
    } else {
      if (elemAreChar) {
        ss << "'" << static_cast<char>(a) << "'..'" << static_cast<char>(b)
           << "'";
      } else {
        ss << a << ".." << b;
      }
    }
  }
  if (effectiveSize > 1) {
    ss << "}";
  }

  return ss.str();
}

std::string IntervalSet::toString(
    const std::vector<std::string>& tokenNames) const {
  return toString(dfa::Vocabulary::fromTokenNames(tokenNames));
}

std::string IntervalSet::toString(const dfa::Vocabulary& vocabulary) const {
  if (_intervals.empty()) {
    return "{}";
  }

  std::stringstream ss;
  size_t effectiveSize = size();
  if (effectiveSize > 1) {
    ss << "{";
  }

  bool firstEntry = true;
  for (auto& interval : _intervals) {
    if (!firstEntry) ss << ", ";
    firstEntry = false;

    ssize_t a = interval.a;
    ssize_t b = interval.b;
    if (a == b) {
      ss << elementName(vocabulary, a);
    } else {
      for (ssize_t i = a; i <= b; i++) {
        if (i > a) {
          ss << ", ";
        }
        ss << elementName(vocabulary, i);
      }
    }
  }
  if (effectiveSize > 1) {
    ss << "}";
  }

  return ss.str();
}

std::string IntervalSet::elementName(const std::vector<std::string>& tokenNames,
                                     ssize_t a) const {
  return elementName(dfa::Vocabulary::fromTokenNames(tokenNames), a);
}

std::string IntervalSet::elementName(const dfa::Vocabulary& vocabulary,
                                     ssize_t a) const {
  if (a == -1) {
    return "<EOF>";
  } else if (a == -2) {
    return "<EPSILON>";
  } else {
    return vocabulary.getDisplayName(a);
  }
}

size_t IntervalSet::size() const {
  size_t result = 0;
  for (auto& interval : _intervals) {
    result += size_t(interval.b - interval.a + 1);
  }
  return result;
}

std::vector<ssize_t> IntervalSet::toList() const {
  std::vector<ssize_t> result;
  for (auto& interval : _intervals) {
    ssize_t a = interval.a;
    ssize_t b = interval.b;
    for (ssize_t v = a; v <= b; v++) {
      result.push_back(v);
    }
  }
  return result;
}

std::set<ssize_t> IntervalSet::toSet() const {
  std::set<ssize_t> result;
  for (auto& interval : _intervals) {
    ssize_t a = interval.a;
    ssize_t b = interval.b;
    for (ssize_t v = a; v <= b; v++) {
      result.insert(v);
    }
  }
  return result;
}

ssize_t IntervalSet::get(size_t i) const {
  size_t index = 0;
  for (auto& interval : _intervals) {
    ssize_t a = interval.a;
    ssize_t b = interval.b;
    for (ssize_t v = a; v <= b; v++) {
      if (index == i) {
        return v;
      }
      index++;
    }
  }
  return -1;
}

void IntervalSet::remove(size_t el) { remove(symbolToNumeric(el)); }

void IntervalSet::remove(ssize_t el) {
  for (size_t i = 0; i < _intervals.size(); ++i) {
    Interval& interval = _intervals[i];
    ssize_t a = interval.a;
    ssize_t b = interval.b;
    if (el < a) {
      break;  // list is sorted and el is before this interval; not here
    }

    // if whole interval x..x, rm
    if (el == a && el == b) {
      _intervals.erase(_intervals.begin() + (long)i);
      break;
    }
    // if on left edge x..b, adjust left
    if (el == a) {
      interval.a++;
      break;
    }
    // if on right edge a..x, adjust right
    if (el == b) {
      interval.b--;
      break;
    }
    // if in middle a..x..b, split interval
    if (el > a && el < b) {  // found in this interval
      ssize_t oldb = interval.b;
      interval.b = el - 1;  // [a..x-1]
      add(el + 1, oldb);    // add [x+1..b]

      break;  // ml: not in the Java code but I believe we also should stop
              // searching here, as we found x.
    }
  }
}
