// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Escape '/'.
function testEscapes(expected, regexp) {
  assertEquals(expected, regexp.source);
  assertEquals("/" + expected + "/", regexp.toString());
}

testEscapes("\\/", /\//);
testEscapes("\\/\\/", /\/\//);
testEscapes("\\/", new RegExp("/"));
testEscapes("\\/", new RegExp("\\/"));
testEscapes("\\\\\\/", new RegExp("\\\\/"));
testEscapes("\\/\\/", new RegExp("\\/\\/"));
testEscapes("\\/\\/\\/\\/", new RegExp("////"));
testEscapes("\\/\\/\\/\\/", new RegExp("\\//\\//"));
testEscapes("(?:)", new RegExp(""));

// Read-only property.
var r = /\/\//;
testEscapes("\\/\\/", r);
r.source = "garbage";
testEscapes("\\/\\/", r);
