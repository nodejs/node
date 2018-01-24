// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/visitors.h"

#include "src/objects/code.h"

namespace v8 {
namespace internal {

#define DECLARE_TAG(ignore1, name, ignore2) name,
const char* const
    VisitorSynchronization::kTags[VisitorSynchronization::kNumberOfSyncTags] = {
        ROOT_ID_LIST(DECLARE_TAG)};
#undef DECLARE_TAG

#define DECLARE_TAG(ignore1, ignore2, name) name,
const char* const VisitorSynchronization::kTagNames
    [VisitorSynchronization::kNumberOfSyncTags] = {ROOT_ID_LIST(DECLARE_TAG)};
#undef DECLARE_TAG

}  // namespace internal
}  // namespace v8
