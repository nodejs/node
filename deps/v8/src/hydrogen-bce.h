// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_BCE_H_
#define V8_HYDROGEN_BCE_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


class BoundsCheckBbData;
class BoundsCheckKey;
class BoundsCheckTable : private ZoneHashMap {
 public:
  explicit BoundsCheckTable(Zone* zone);

  INLINE(BoundsCheckBbData** LookupOrInsert(BoundsCheckKey* key, Zone* zone));
  INLINE(void Insert(BoundsCheckKey* key, BoundsCheckBbData* data, Zone* zone));
  INLINE(void Delete(BoundsCheckKey* key));

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckTable);
};


class HBoundsCheckEliminationPhase : public HPhase {
 public:
  explicit HBoundsCheckEliminationPhase(HGraph* graph)
      : HPhase("H_Bounds checks elimination", graph), table_(zone()) { }

  void Run() {
    EliminateRedundantBoundsChecks(graph()->entry_block());
  }

 private:
  void EliminateRedundantBoundsChecks(HBasicBlock* bb);
  BoundsCheckBbData* PreProcessBlock(HBasicBlock* bb);
  void PostProcessBlock(HBasicBlock* bb, BoundsCheckBbData* data);

  BoundsCheckTable table_;

  DISALLOW_COPY_AND_ASSIGN(HBoundsCheckEliminationPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_BCE_H_
