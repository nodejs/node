// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var s = %CreatePrivateOwnSymbol("s");
var s1 = %CreatePrivateOwnSymbol("s1");

function TestSimple() {
  var p = {}
  p[s] = "moo";

  var o = Object.create(p);

  assertEquals(undefined, o[s]);
  assertEquals("moo", p[s]);

  o[s] = "bow-wow";
  assertEquals("bow-wow", o[s]);
  assertEquals("moo", p[s]);
}

TestSimple();


function TestICs() {
  var p = {}
  p[s] = "moo";


  var o = Object.create(p);
  o[s1] = "bow-wow";
  function checkNonOwn(o) {
    assertEquals(undefined, o[s]);
    assertEquals("bow-wow", o[s1]);
  }

  checkNonOwn(o);

  // Test monomorphic/optimized.
  for (var i = 0; i < 1000; i++) {
    checkNonOwn(o);
  }

  // Test non-monomorphic.
  for (var i = 0; i < 1000; i++) {
    var oNew = Object.create(p);
    oNew["s" + i] = i;
    oNew[s1] = "bow-wow";
    checkNonOwn(oNew);
  }
}

TestICs();
