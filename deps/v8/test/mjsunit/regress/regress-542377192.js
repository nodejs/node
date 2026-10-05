// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --maglev-disable-builtin-reducers

// Constant `0` from-index causes Select to fold into an empty subgraph,
// exercising the empty-subgraph buffer stashing path without mutating
// the iterated block vector mid-visit.
function __f_11(__v_29, __v_30, __v_31) {
    return __v_29.indexOf(__v_30, __v_31);
}
for (var __v_23 = 0; __v_23 < 2e5; ++__v_23) {
    var __v_24 = [];
    assertEquals(-1, __f_11(__v_24, 'abcd', 0));
}
