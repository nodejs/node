// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function sloppyArgumentsGenerator(a,b) {
  arguments[7] = 88;
  return arguments;
}

function testLoad() {
  let a = sloppyArgumentsGenerator(66,55,45,77);
  for (let i=0;i<2;++i) {
    assertEquals(a[0], 66);
  }
  for (let i=0;i<2;++i) {
    assertEquals(a[2], 45);
  }
  for (let i=0;i<2;++i) {
    assertEquals(a[10], undefined);
  }
  for (let i=0;i<2;++i) {
    assertEquals(a[6], undefined);
  }
  for (let i=0;i<2;++i) {
    assertEquals(a[7], 88);
  }
  delete a[0];
  for (let i=0;i<2;++i) {
    assertEquals(a[0], undefined);
  }
}

function testHas() {
  let a = sloppyArgumentsGenerator(66,55,45,77);
  for (let i=0;i<2;++i) {
    assertTrue(0 in a);
  }
  for (let i=0;i<2;++i) {
    assertTrue(2 in a);
  }
  for (let i=0;i<2;++i) {
    assertFalse(10 in a);
  }
  for (let i=0;i<2;++i) {
    assertFalse(6 in a);
  }
  for (let i=0;i<2;++i) {
    assertTrue(7 in a);
  }
  delete a[0];
  for (let i=0;i<2;++i) {
    assertFalse(0 in a);
  }
}

// Test once without type feedback vector
testLoad();
testHas();

%EnsureFeedbackVectorForFunction(testLoad);
%EnsureFeedbackVectorForFunction(testHas);

// Test again with type feedback vector
testLoad();
testHas();
