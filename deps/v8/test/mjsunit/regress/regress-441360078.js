// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:


function PI(PI, ...E) {
    return E[0] + E[[[PI].set].bind - 1];
}
for (E = 0; E < 9986; ++E)
    [[[].set].get].filter(PI);
