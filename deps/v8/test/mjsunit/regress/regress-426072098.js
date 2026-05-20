// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

let arr = [];
// We need to increase the size of the array until it doesn't fit into new
// space anymore.
for(let i = 0; i < 10000; ++i) {
 arr.push(undefined, 10);
}
