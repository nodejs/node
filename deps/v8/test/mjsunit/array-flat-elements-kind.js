// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertSmiElementsKind(arr) {
  assertTrue(%HasSmiElements(arr),
      'Expected SMI elements kind, got: ' + JSON.stringify(arr));
}

function assertDoubleElementsKind(arr) {
  assertTrue(%HasDoubleElements(arr),
      'Expected DOUBLE elements kind, got: ' + JSON.stringify(arr));
}

function assertObjectElementsKind(arr) {
  assertTrue(%HasObjectElements(arr),
      'Expected OBJECT elements kind, got: ' + JSON.stringify(arr));
}

// Early return for packed numeric source.
assertSmiElementsKind([1].flat());
assertDoubleElementsKind([1.1].flat());

// All sub-arrays have the same elements kind.
assertSmiElementsKind([[1]].flat());
assertSmiElementsKind([[1],[1]].flat());
assertDoubleElementsKind([[1.1]].flat());
assertDoubleElementsKind([[1.1],[2.2]].flat());

// Mixed sub-array kinds produce DOUBLE (SMIs fit in doubles).
assertDoubleElementsKind([[1],[1.1]].flat());
assertObjectElementsKind([[1],[[1.1]]].flat());

// Non-number leaf elements.
assertObjectElementsKind([["hello"]].flat());
assertObjectElementsKind([[1, "hello"]].flat());

// Generic source with Smi-only contents.
let generic = [{}];
generic[0] = 1;
assertSmiElementsKind(generic.flat());
assertSmiElementsKind([generic].flat());

// Depth > 1.
assertSmiElementsKind([[[1]]].flat(2));
assertDoubleElementsKind([[[1.1],[2.2]]].flat(2));
