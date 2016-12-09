// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// First test case

function FirstBuffer () {}
FirstBuffer.prototype.__proto__ = Uint8Array.prototype
FirstBuffer.__proto__ = Uint8Array

var buf = new Uint8Array(10)
buf.__proto__ = FirstBuffer.prototype

var buf2 = buf.subarray(2)
assertEquals(8, buf2.length);

// Second test case

function SecondBuffer (arg) {
  var arr = new Uint8Array(arg)
  arr.__proto__ = SecondBuffer.prototype
  return arr
}
SecondBuffer.prototype.__proto__ = Uint8Array.prototype
SecondBuffer.__proto__ = Uint8Array

var buf3 = new SecondBuffer(10)

var buf4 = buf3.subarray(2)

assertEquals(8, buf4.length);
