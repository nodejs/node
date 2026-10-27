// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

function TestBrandChecks(struct1, struct2, sa) {
  assertTrue(SharedStructType.isSharedStruct(struct1));
  assertTrue(SharedStructType.isSharedStruct(struct2));
  assertFalse(SharedStructType.isSharedStruct(sa));

  assertTrue(SharedArray.isSharedArray(sa));
  assertTrue(sa instanceof SharedArray);
  for (const x of [struct1, struct2]) {
    assertFalse(SharedArray.isSharedArray(x));
  }

  const falsehoods = [42, -0, undefined, null, true, false, 'foo', {}, []];
  for (const x of falsehoods) {
    assertFalse(SharedStructType.isSharedStruct(x));
    assertFalse(SharedArray.isSharedArray(x));
  }
}

// Test same Realm.
const S1 = new SharedStructType(['field']);
const S2 = new SharedStructType(['field2']);
const struct1 = new S1();
const struct2 = new S2();
const sa = new SharedArray(1);
TestBrandChecks(struct1, struct2, sa);

// Test cross-Realm.
const [otherStruct1, otherStruct2, otherSa] =
    Realm.eval(Realm.create(), `const S1 = new SharedStructType(['field']);
const S2 = new SharedStructType(['field2']);
const struct1 = new S1();
const struct2 = new S2();
const sa = new SharedArray(1);
[struct1, struct2, sa]`);
TestBrandChecks(otherStruct1, otherStruct2, otherSa);
