// Copyright Node.js contributors. All rights reserved.
// SPDX-License-Identifier: MIT

#include "string_search.h"

namespace node {
namespace stringsearch {

int StringSearchBase::kBadCharShiftTable[kUC16AlphabetSize];
int StringSearchBase::kGoodSuffixShiftTable[kBMMaxShift + 1];
int StringSearchBase::kSuffixTable[kBMMaxShift + 1];

}  // namespace stringsearch
}  // namespace node
