// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_CHECK_ELIMINATION_H_
#define V8_HYDROGEN_CHECK_ELIMINATION_H_

#include "src/hydrogen.h"
#include "src/hydrogen-alias-analysis.h"

namespace v8 {
namespace internal {


// Remove CheckMaps instructions through flow- and branch-sensitive analysis.
class HCheckEliminationPhase : public HPhase {
 public:
  explicit HCheckEliminationPhase(HGraph* graph)
      : HPhase("H_Check Elimination", graph), aliasing_(),
        string_maps_(kStringMapsSize, zone()) {
    // Compute the set of string maps.
    #define ADD_STRING_MAP(type, size, name, Name)                  \
      string_maps_.Add(Unique<Map>::CreateImmovable(                \
              graph->isolate()->factory()->name##_map()), zone());
    STRING_TYPE_LIST(ADD_STRING_MAP)
    #undef ADD_STRING_MAP
    DCHECK_EQ(kStringMapsSize, string_maps_.size());
#ifdef DEBUG
    redundant_ = 0;
    removed_ = 0;
    removed_cho_ = 0;
    removed_cit_ = 0;
    narrowed_ = 0;
    loads_ = 0;
    empty_ = 0;
    compares_true_ = 0;
    compares_false_ = 0;
    transitions_ = 0;
#endif
  }

  void Run();

  friend class HCheckTable;

 private:
  const UniqueSet<Map>* string_maps() const { return &string_maps_; }

  void PrintStats();

  HAliasAnalyzer* aliasing_;
  #define COUNT(type, size, name, Name) + 1
  static const int kStringMapsSize = 0 STRING_TYPE_LIST(COUNT);
  #undef COUNT
  UniqueSet<Map> string_maps_;
#ifdef DEBUG
  int redundant_;
  int removed_;
  int removed_cho_;
  int removed_cit_;
  int narrowed_;
  int loads_;
  int empty_;
  int compares_true_;
  int compares_false_;
  int transitions_;
#endif
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_CHECK_ELIMINATION_H_
