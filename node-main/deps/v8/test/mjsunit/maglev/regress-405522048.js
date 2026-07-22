// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --maglev-non-eager-inlining --max-maglev-inlined-bytecode-size-small=0

deepEquals = function deepEquals( b) {
  if (typeof a !== typeof b) return false;
  switch (objectClass) {
  }
};
assertEquals = function assertEquals(expected) {
  if (!deepEquals( expected)) {
  }
};
__f_9(256);
function __f_9(__v_34) {
  var __v_35 = 'obj = {';
  __v_35 += '};';
try {
  eval(__v_35);
  for (var __v_37 = 0; __v_37 <= __v_34; __v_37++) {
    assertEquals(__v_37, obj['k' + __v_37]);
  }
} catch (e) {}
}
