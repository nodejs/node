/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Recognizer.h"
#include "atn/ATN.h"
#include "atn/ATNState.h"

namespace antlr4 {
namespace atn {

struct PredictionContextHasher;
struct PredictionContextComparer;
class PredictionContextMergeCache;

typedef std::unordered_set<Ref<PredictionContext>, PredictionContextHasher,
                           PredictionContextComparer>
    PredictionContextCache;

class ANTLR4CPP_PUBLIC PredictionContext {
 public:
  /// Represents $ in local context prediction, which means wildcard.
  /// *+x = *.
  static const Ref<PredictionContext> EMPTY;

  /// Represents $ in an array in full context mode, when $
  /// doesn't mean wildcard: $ + x = [$,x]. Here,
  /// $ = EMPTY_RETURN_STATE.
  // ml: originally Integer.MAX_VALUE, which would be -1 for us, but this is
  // already used in places where
  //     -1 is converted to unsigned, so we use a different value here. Any
  //     value does the job provided it doesn't conflict with real return
  //     states.
  static const size_t EMPTY_RETURN_STATE =
      static_cast<size_t>(-10);  // std::numeric_limits<size_t>::max() - 9;

 private:
  static const size_t INITIAL_HASH = 1;

 public:
  static size_t globalNodeCount;
  const size_t id;

  /// <summary>
  /// Stores the computed hash code of this <seealso cref="PredictionContext"/>.
  /// The hash code is computed in parts to match the following reference
  /// algorithm.
  ///
  /// <pre>
  ///  private int referenceHashCode() {
  ///      int hash = <seealso cref="MurmurHash#initialize"/>(<seealso
  ///      cref="#INITIAL_HASH"/>);
  ///
  ///      for (int i = 0; i < <seealso cref="#size()"/>; i++) {
  ///          hash = <seealso cref="MurmurHash#update"/>(hash, <seealso
  ///          cref="#getParent"/>(i));
  ///      }
  ///
  ///      for (int i = 0; i < <seealso cref="#size()"/>; i++) {
  ///          hash = <seealso cref="MurmurHash#update"/>(hash, <seealso
  ///          cref="#getReturnState"/>(i));
  ///      }
  ///
  ///      hash = <seealso cref="MurmurHash#finish"/>(hash, 2 * <seealso
  ///      cref="#size()"/>); return hash;
  ///  }
  /// </pre>
  /// </summary>
  const size_t cachedHashCode;

 protected:
  PredictionContext(size_t cachedHashCode);
  ~PredictionContext();

 public:
  /// Convert a RuleContext tree to a PredictionContext graph.
  /// Return EMPTY if outerContext is empty.
  static Ref<PredictionContext> fromRuleContext(const ATN& atn,
                                                RuleContext* outerContext);

  virtual size_t size() const = 0;
  virtual Ref<PredictionContext> getParent(size_t index) const = 0;
  virtual size_t getReturnState(size_t index) const = 0;

  virtual bool operator==(const PredictionContext& o) const = 0;

  /// This means only the EMPTY (wildcard? not sure) context is in set.
  virtual bool isEmpty() const;
  virtual bool hasEmptyPath() const;
  virtual size_t hashCode() const;

 protected:
  static size_t calculateEmptyHashCode();
  static size_t calculateHashCode(Ref<PredictionContext> parent,
                                  size_t returnState);
  static size_t calculateHashCode(
      const std::vector<Ref<PredictionContext>>& parents,
      const std::vector<size_t>& returnStates);

 public:
  // dispatch
  static Ref<PredictionContext> merge(const Ref<PredictionContext>& a,
                                      const Ref<PredictionContext>& b,
                                      bool rootIsWildcard,
                                      PredictionContextMergeCache* mergeCache);

  /// <summary>
  /// Merge two <seealso cref="SingletonPredictionContext"/> instances.
  ///
  /// <p/>
  ///
  /// Stack tops equal, parents merge is same; return left graph.<br/>
  /// <embed src="images/SingletonMerge_SameRootSamePar.svg"
  /// type="image/svg+xml"/>
  ///
  /// <p/>
  ///
  /// Same stack top, parents differ; merge parents giving array node, then
  /// remainders of those graphs. A new root node is created to point to the
  /// merged parents.<br/>
  /// <embed src="images/SingletonMerge_SameRootDiffPar.svg"
  /// type="image/svg+xml"/>
  ///
  /// <p/>
  ///
  /// Different stack tops pointing to same parent. Make array node for the
  /// root where both element in the root point to the same (original)
  /// parent.<br/>
  /// <embed src="images/SingletonMerge_DiffRootSamePar.svg"
  /// type="image/svg+xml"/>
  ///
  /// <p/>
  ///
  /// Different stack tops pointing to different parents. Make array node for
  /// the root where each element points to the corresponding original
  /// parent.<br/>
  /// <embed src="images/SingletonMerge_DiffRootDiffPar.svg"
  /// type="image/svg+xml"/>
  /// </summary>
  /// <param name="a"> the first <seealso cref="SingletonPredictionContext"/>
  /// </param> <param name="b"> the second <seealso
  /// cref="SingletonPredictionContext"/> </param> <param name="rootIsWildcard">
  /// {@code true} if this is a local-context merge, otherwise false to indicate
  /// a full-context merge </param> <param name="mergeCache"> </param>
  static Ref<PredictionContext> mergeSingletons(
      const Ref<SingletonPredictionContext>& a,
      const Ref<SingletonPredictionContext>& b, bool rootIsWildcard,
      PredictionContextMergeCache* mergeCache);

  /**
   * Handle case where at least one of {@code a} or {@code b} is
   * {@link #EMPTY}. In the following diagrams, the symbol {@code $} is used
   * to represent {@link #EMPTY}.
   *
   * <h2>Local-Context Merges</h2>
   *
   * <p>These local-context merge operations are used when {@code
   * rootIsWildcard} is true.</p>
   *
   * <p>{@link #EMPTY} is superset of any graph; return {@link #EMPTY}.<br>
   * <embed src="images/LocalMerge_EmptyRoot.svg" type="image/svg+xml"/></p>
   *
   * <p>{@link #EMPTY} and anything is {@code #EMPTY}, so merged parent is
   * {@code #EMPTY}; return left graph.<br>
   * <embed src="images/LocalMerge_EmptyParent.svg" type="image/svg+xml"/></p>
   *
   * <p>Special case of last merge if local context.<br>
   * <embed src="images/LocalMerge_DiffRoots.svg" type="image/svg+xml"/></p>
   *
   * <h2>Full-Context Merges</h2>
   *
   * <p>These full-context merge operations are used when {@code rootIsWildcard}
   * is false.</p>
   *
   * <p><embed src="images/FullMerge_EmptyRoots.svg" type="image/svg+xml"/></p>
   *
   * <p>Must keep all contexts; {@link #EMPTY} in array is a special value (and
   * null parent).<br>
   * <embed src="images/FullMerge_EmptyRoot.svg" type="image/svg+xml"/></p>
   *
   * <p><embed src="images/FullMerge_SameRoot.svg" type="image/svg+xml"/></p>
   *
   * @param a the first {@link SingletonPredictionContext}
   * @param b the second {@link SingletonPredictionContext}
   * @param rootIsWildcard {@code true} if this is a local-context merge,
   * otherwise false to indicate a full-context merge
   */
  static Ref<PredictionContext> mergeRoot(
      const Ref<SingletonPredictionContext>& a,
      const Ref<SingletonPredictionContext>& b, bool rootIsWildcard);

  /**
   * Merge two {@link ArrayPredictionContext} instances.
   *
   * <p>Different tops, different parents.<br>
   * <embed src="images/ArrayMerge_DiffTopDiffPar.svg"
   * type="image/svg+xml"/></p>
   *
   * <p>Shared top, same parents.<br>
   * <embed src="images/ArrayMerge_ShareTopSamePar.svg"
   * type="image/svg+xml"/></p>
   *
   * <p>Shared top, different parents.<br>
   * <embed src="images/ArrayMerge_ShareTopDiffPar.svg"
   * type="image/svg+xml"/></p>
   *
   * <p>Shared top, all shared parents.<br>
   * <embed src="images/ArrayMerge_ShareTopSharePar.svg"
   * type="image/svg+xml"/></p>
   *
   * <p>Equal tops, merge parents and reduce top to
   * {@link SingletonPredictionContext}.<br>
   * <embed src="images/ArrayMerge_EqualTop.svg" type="image/svg+xml"/></p>
   */
  static Ref<PredictionContext> mergeArrays(
      const Ref<ArrayPredictionContext>& a,
      const Ref<ArrayPredictionContext>& b, bool rootIsWildcard,
      PredictionContextMergeCache* mergeCache);

 protected:
  /// Make pass over all M parents; merge any equal() ones.
  /// @returns true if the list has been changed (i.e. duplicates where found).
  static bool combineCommonParents(
      std::vector<Ref<PredictionContext>>& parents);

 public:
  static std::string toDOTString(const Ref<PredictionContext>& context);

  static Ref<PredictionContext> getCachedContext(
      const Ref<PredictionContext>& context,
      PredictionContextCache& contextCache,
      std::map<Ref<PredictionContext>, Ref<PredictionContext>>& visited);

  // ter's recursive version of Sam's getAllNodes()
  static std::vector<Ref<PredictionContext>> getAllContextNodes(
      const Ref<PredictionContext>& context);
  static void getAllContextNodes_(const Ref<PredictionContext>& context,
                                  std::vector<Ref<PredictionContext>>& nodes,
                                  std::set<PredictionContext*>& visited);

  virtual std::string toString() const;
  virtual std::string toString(Recognizer* recog) const;

  std::vector<std::string> toStrings(Recognizer* recognizer, int currentState);
  std::vector<std::string> toStrings(Recognizer* recognizer,
                                     const Ref<PredictionContext>& stop,
                                     int currentState);
};

struct PredictionContextHasher {
  size_t operator()(const Ref<PredictionContext>& k) const {
    return k->hashCode();
  }
};

struct PredictionContextComparer {
  bool operator()(const Ref<PredictionContext>& lhs,
                  const Ref<PredictionContext>& rhs) const {
    if (lhs == rhs)  // Object identity.
      return true;
    return (lhs->hashCode() == rhs->hashCode()) && (*lhs == *rhs);
  }
};

class PredictionContextMergeCache {
 public:
  Ref<PredictionContext> put(Ref<PredictionContext> const& key1,
                             Ref<PredictionContext> const& key2,
                             Ref<PredictionContext> const& value);
  Ref<PredictionContext> get(Ref<PredictionContext> const& key1,
                             Ref<PredictionContext> const& key2);

  void clear();
  std::string toString() const;
  size_t count() const;

 private:
  std::unordered_map<
      Ref<PredictionContext>,
      std::unordered_map<Ref<PredictionContext>, Ref<PredictionContext>,
                         PredictionContextHasher, PredictionContextComparer>,
      PredictionContextHasher, PredictionContextComparer>
      _data;
};

}  // namespace atn
}  // namespace antlr4
