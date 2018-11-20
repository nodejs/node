/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/DecisionEventInfo.h"

using namespace antlr4;
using namespace antlr4::atn;

DecisionEventInfo::DecisionEventInfo(size_t decision, ATNConfigSet* configs,
                                     TokenStream* input, size_t startIndex,
                                     size_t stopIndex, bool fullCtx)
    : decision(decision),
      configs(configs),
      input(input),
      startIndex(startIndex),
      stopIndex(stopIndex),
      fullCtx(fullCtx) {}
