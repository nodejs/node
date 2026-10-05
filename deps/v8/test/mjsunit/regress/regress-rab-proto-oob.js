// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

function testOobProtoWrite(n) {
  let src = `o["${n}"] = 42`;
  let setElem = new Function("o", src)
  var rab = new ArrayBuffer(n+1, { maxByteLength: n+1 });
  var ta  = new Int8Array(rab);
  for (var i = 0; i < 100; i++) setElem(Object.create(ta));
  rab.resize(8);
  var victim = Object.create(ta);
  setElem(victim);
  assertEquals(undefined, victim[n]);
}

testOobProtoWrite(1000);

if (%Is64Bit()) {
  testOobProtoWrite(0x100000000);
}

function testOobProtoWriteManyViews(n) {
  let src = `o["${n}"] = 42`;
  let setElem = new Function("o", src)
  var rab = new ArrayBuffer(n+1, { maxByteLength: n+1 });
  var ta  = new Int8Array(rab);
  var ta2 = new Int8Array(rab); // Second view to trigger kManyViews
  for (var i = 0; i < 100; i++) setElem(Object.create(ta));
  rab.resize(8);
  var victim = Object.create(ta);
  setElem(victim);
  assertEquals(undefined, victim[n]);
}

testOobProtoWriteManyViews(1000);
