// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function EnsureDictionaryMode(obj) {
  obj.foo = 0;
  obj.bar = 0;
  // Delete the second-to-last property first to force normalization.
  delete obj.foo;
  delete obj.bar;
  assertFalse(%HasFastProperties(obj));
}

var o = Object.seal([]);
assertTrue(%HasFastProperties(o));
assertTrue(Object.isSealed(o));
assertFalse(Object.isFrozen(o));

var o = [];
EnsureDictionaryMode(o);
Object.seal(o);
%DebugPrint(o);
assertFalse(%HasFastProperties(o));
assertTrue(Object.isSealed(o));
assertFalse(Object.isFrozen(o));
