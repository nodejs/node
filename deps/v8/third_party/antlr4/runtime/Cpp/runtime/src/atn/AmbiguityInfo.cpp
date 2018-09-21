/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/AmbiguityInfo.h"

using namespace antlr4;
using namespace antlr4::atn;

AmbiguityInfo::AmbiguityInfo(size_t decision, ATNConfigSet* configs,
                             const antlrcpp::BitSet& ambigAlts,
                             TokenStream* input, size_t startIndex,
                             size_t stopIndex, bool fullCtx)
    : DecisionEventInfo(decision, configs, input, startIndex, stopIndex,
                        fullCtx) {
  this->ambigAlts = ambigAlts;
}
