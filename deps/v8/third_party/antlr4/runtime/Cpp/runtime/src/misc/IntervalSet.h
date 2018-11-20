/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Exceptions.h"
#include "misc/Interval.h"

namespace antlr4 {
namespace misc {

/**
 * This class implements the {@link IntSet} backed by a sorted array of
 * non-overlapping intervals. It is particularly efficient for representing
 * large collections of numbers, where the majority of elements appear as part
 * of a sequential range of numbers that are all part of the set. For example,
 * the set { 1, 2, 3, 4, 7, 8 } may be represented as { [1, 4], [7, 8] }.
 *
 * <p>
 * This class is able to represent sets containing any combination of values in
 * the range {@link Integer#MIN_VALUE} to {@link Integer#MAX_VALUE}
 * (inclusive).</p>
 */
class ANTLR4CPP_PUBLIC IntervalSet {
 public:
  static IntervalSet const COMPLETE_CHAR_SET;
  static IntervalSet const EMPTY_SET;

 private:
  /// The list of sorted, disjoint intervals.
  std::vector<Interval> _intervals;

  explicit IntervalSet(std::vector<Interval>&& intervals);

 public:
  IntervalSet();
  IntervalSet(IntervalSet const& set);
  IntervalSet(IntervalSet&& set);

  template <typename T1, typename... T_NEXT>
  IntervalSet(int, T1 t1, T_NEXT&&... next) : IntervalSet() {
    // The first int argument is an ignored count for compatibility
    // with the previous varargs based interface.
    addItems(t1, std::forward<T_NEXT>(next)...);
  }

  IntervalSet& operator=(IntervalSet const& set);
  IntervalSet& operator=(IntervalSet&& set);

  /// Create a set with a single element, el.
  static IntervalSet of(ssize_t a);

  /// Create a set with all ints within range [a..b] (inclusive)
  static IntervalSet of(ssize_t a, ssize_t b);

  void clear();

  /// Add a single element to the set.  An isolated element is stored
  /// as a range el..el.
  void add(ssize_t el);

  /// Add interval; i.e., add all integers from a to b to set.
  /// If b<a, do nothing.
  /// Keep list in sorted order (by left range value).
  /// If overlap, combine ranges.  For example,
  /// If this is {1..5, 10..20}, adding 6..7 yields
  /// {1..5, 6..7, 10..20}.  Adding 4..8 yields {1..8, 10..20}.
  void add(ssize_t a, ssize_t b);

  /// combine all sets in the array returned the or'd value
  static IntervalSet Or(const std::vector<IntervalSet>& sets);

  // Copy on write so we can cache a..a intervals and sets of that.
  void add(const Interval& addition);
  IntervalSet& addAll(const IntervalSet& set);

  template <typename T1, typename... T_NEXT>
  void addItems(T1 t1, T_NEXT&&... next) {
    add(t1);
    addItems(std::forward<T_NEXT>(next)...);
  }

  IntervalSet complement(ssize_t minElement, ssize_t maxElement) const;

  /// Given the set of possible values (rather than, say UNICODE or MAXINT),
  /// return a new set containing all elements in vocabulary, but not in
  /// this.  The computation is (vocabulary - this).
  ///
  /// 'this' is assumed to be either a subset or equal to vocabulary.
  IntervalSet complement(const IntervalSet& vocabulary) const;

  /// Compute this-other via this&~other.
  /// Return a new set containing all elements in this but not in other.
  /// other is assumed to be a subset of this;
  /// anything that is in other but not in this will be ignored.
  IntervalSet subtract(const IntervalSet& other) const;

  /**
   * Compute the set difference between two interval sets. The specific
   * operation is {@code left - right}. If either of the input sets is
   * {@code null}, it is treated as though it was an empty set.
   */
  static IntervalSet subtract(const IntervalSet& left,
                              const IntervalSet& right);

  IntervalSet Or(const IntervalSet& a) const;

  /// Return a new set with the intersection of this set with other.  Because
  /// the intervals are sorted, we can use an iterator for each list and
  /// just walk them together.  This is roughly O(min(n,m)) for interval
  /// list lengths n and m.
  IntervalSet And(const IntervalSet& other) const;

  /// Is el in any range of this set?
  bool contains(size_t el) const;  // For mapping of e.g. Token::EOF to -1 etc.
  bool contains(ssize_t el) const;

  /// return true if this set has no members
  bool isEmpty() const;

  /// If this set is a single integer, return it otherwise Token.INVALID_TYPE.
  ssize_t getSingleElement() const;

  /**
   * Returns the maximum value contained in the set.
   *
   * @return the maximum value contained in the set. If the set is empty, this
   * method returns {@link Token#INVALID_TYPE}.
   */
  ssize_t getMaxElement() const;

  /**
   * Returns the minimum value contained in the set.
   *
   * @return the minimum value contained in the set. If the set is empty, this
   * method returns {@link Token#INVALID_TYPE}.
   */
  ssize_t getMinElement() const;

  /// <summary>
  /// Return a list of Interval objects. </summary>
  std::vector<Interval> const& getIntervals() const;

  size_t hashCode() const;

  /// Are two IntervalSets equal?  Because all intervals are sorted
  ///  and disjoint, equals is a simple linear walk over both lists
  ///  to make sure they are the same.
  bool operator==(const IntervalSet& other) const;
  std::string toString() const;
  std::string toString(bool elemAreChar) const;

  /**
   * @deprecated Use {@link #toString(Vocabulary)} instead.
   */
  std::string toString(const std::vector<std::string>& tokenNames) const;
  std::string toString(const dfa::Vocabulary& vocabulary) const;

 protected:
  /**
   * @deprecated Use {@link #elementName(Vocabulary, int)} instead.
   */
  std::string elementName(const std::vector<std::string>& tokenNames,
                          ssize_t a) const;
  std::string elementName(const dfa::Vocabulary& vocabulary, ssize_t a) const;

 public:
  size_t size() const;
  std::vector<ssize_t> toList() const;
  std::set<ssize_t> toSet() const;

  /// Get the ith element of ordered set.  Used only by RandomPhrase so
  /// don't bother to implement if you're not doing that for a new
  /// ANTLR code gen target.
  ssize_t get(size_t i) const;
  void remove(size_t el);  // For mapping of e.g. Token::EOF to -1 etc.
  void remove(ssize_t el);

 private:
  void addItems() { /* No-op */
  }
};

}  // namespace misc
}  // namespace antlr4

// Hash function for IntervalSet.

namespace std {
using antlr4::misc::IntervalSet;

template <>
struct hash<IntervalSet> {
  size_t operator()(const IntervalSet& x) const { return x.hashCode(); }
};
}  // namespace std
