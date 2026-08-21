// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(){
const array = new Uint8Array(8);
const view = new DataView(array.buffer);
const value = 123456;

  view.setInt32(0, value, false);
  assertEquals(0x0, array[0]);
  assertEquals(0x1, array[1]);
  assertEquals(0xe2, array[2]);
  assertEquals(0x40, array[3]);
  assertEquals(value, view.getInt32(0, false));

  view.setInt32(0, value, true);
  assertEquals(0x40, array[0]);
  assertEquals(0xe2, array[1]);
  assertEquals(0x1, array[2]);
  assertEquals(0x0, array[3]);
  assertEquals(value, view.getInt32(0, true));

  view.setFloat64(0, value, false);
  assertEquals(0x40, array[0]);
  assertEquals(0xfe, array[1]);
  assertEquals(0x24, array[2]);
  assertEquals(0x0, array[3]);
  assertEquals(value, view.getFloat64(0, false));

  view.setFloat64(0, value, true);
  assertEquals(0x0, array[4]);
  assertEquals(0x24, array[5]);
  assertEquals(0xfe, array[6]);
  assertEquals(0x40, array[7]);
  assertEquals(value, view.getFloat64(0, true));
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
