// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function makeArray(x) { return [x]; }

let i = 0;
for (const v of [-undefined, true]) {
  function bar(x) {
    makeArray(undefined);
    return makeArray(x);
  }

  const result = bar(v);
  if(i == 0) assertEquals([NaN], result);
  if(i == 1) assertEquals([true], result);
  ++i;
}
