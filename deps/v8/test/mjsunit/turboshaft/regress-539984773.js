// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --turbofan --no-turbolev
// Flags: --no-optimize-on-next-call-optimizes-to-maglev
// Flags: --concurrent-recompilation --no-stress-background-compile

function makeSeqOneByte(seed) {
  const chars = [];
  for (let i = 0; i < 64; ++i) chars.push(65 + ((seed + i) % 23));
  return String.fromCharCode.apply(null, chars);
}

// Ensure canonicalTwin is internalized in the string table first.
const canonicalTwin = makeSeqOneByte(0);
(new Map()).has(canonicalTwin);

// Create an identical but non-internalized SeqOneByteString.
const key = makeSeqOneByte(0);

function constantCharCodeAt(i) {
  return key.charCodeAt(i);
}

const expected = [];
for (let i = 0; i < 64; ++i) expected.push(key.charCodeAt(i));

%PrepareFunctionForOptimization(constantCharCodeAt);
constantCharCodeAt(8);
constantCharCodeAt(8);

// Block background compilation at TurboshaftMachineLowering phase.
%BlockAt('TurboshaftMachineLowering', 10000);
%OptimizeFunctionOnNextCall(constantCharCodeAt, "concurrent");
constantCharCodeAt(8);

// Wait until background compiler thread reaches TurboshaftMachineLowering.
assertTrue(%WaitUntilBlocked('TurboshaftMachineLowering', 10000));

// Internalize 'key' on the main thread while background lowering is stopped.
// Since canonicalTwin is in the string table, 'key' transitions in-place
// from SeqOneByteString to a 16-byte ThinString.
(new Map()).has(key);
gc();

// Resume background compiler thread and wait for compilation to finish.
assertTrue(%Resume('TurboshaftMachineLowering'));
%WaitForBackgroundOptimization();

// Verify that all character codes are correct and no out-of-bounds read occurred.
for (let i = 0; i < 64; ++i) {
  assertEquals(expected[i], constantCharCodeAt(i));
}
