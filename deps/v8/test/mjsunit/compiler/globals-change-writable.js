// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan


// Test Constant cell value going from writable to read-only.
{
  glo4 = 4;

  function write_glo4(x) { glo4 = x }

  %PrepareFunctionForOptimization(write_glo4);
  write_glo4(4);
  assertEquals(4, glo4);

  // At this point, glo4 has cell type Constant.

  %OptimizeFunctionOnNextCall(write_glo4);
  write_glo4(4);
  assertEquals(4, glo4);
  assertOptimized(write_glo4);

  Object.defineProperty(this, 'glo4', {writable: false});
  assertUnoptimized(write_glo4);
  write_glo4(2);
  assertEquals(4, glo4);
}


// Test ConstantType cell value going from writable to read-only.
{
  glo5 = 5;

  function write_glo5(x) { glo5 = x }

  %PrepareFunctionForOptimization(write_glo5);
  write_glo5(0);
  assertEquals(0, glo5);

  // At this point, glo5 has cell type ConstantType.

  %OptimizeFunctionOnNextCall(write_glo5);
  write_glo5(5);
  assertEquals(5, glo5);
  assertOptimized(write_glo5);

  Object.defineProperty(this, 'glo5', {writable: false});
  assertUnoptimized(write_glo5);
  write_glo5(2);
  assertEquals(5, glo5);
}


// Test Mutable cell value going from writable to read-only.
{
  glo6 = 6;

  function write_glo6(x) { glo6 = x }

  %PrepareFunctionForOptimization(write_glo6);
  write_glo6({});
  write_glo6(3);
  assertEquals(3, glo6);

  // At this point, glo6 has cell type Mutable.

  %OptimizeFunctionOnNextCall(write_glo6);
  write_glo6(6);
  assertEquals(6, glo6);
  assertOptimized(write_glo6);

  Object.defineProperty(this, 'glo6', {writable: false});
  assertUnoptimized(write_glo6);
  write_glo6(2);
  assertEquals(6, glo6);
}


// Test Constant cell value going from read-only to writable.
{
  glo7 = 7;
  Object.defineProperty(this, 'glo7', {writable: false});

  function read_glo7() { return glo7 }

  %PrepareFunctionForOptimization(read_glo7);
  assertEquals(7, read_glo7());

  // At this point, glo7 has cell type Constant.

  %OptimizeFunctionOnNextCall(read_glo7);
  assertEquals(7, read_glo7());

  Object.defineProperty(this, 'glo7', {writable: true});
  assertEquals(7, read_glo7());
  assertOptimized(read_glo7);
}


// Test ConstantType cell value going from read-only to writable.
{
  glo8 = 0;
  glo8 = 8;
  Object.defineProperty(this, 'glo8', {writable: false});

  function read_glo8() { return glo8 }

  %PrepareFunctionForOptimization(read_glo8);
  assertEquals(8, read_glo8());

  // At this point, glo8 has cell type ConstantType.

  %OptimizeFunctionOnNextCall(read_glo8);
  assertEquals(8, read_glo8());

  Object.defineProperty(this, 'glo8', {writable: true});
  assertEquals(8, read_glo8());
  assertOptimized(read_glo8);
}


// Test Mutable cell value going from read-only to writable.
{
  glo9 = {};
  glo9 = 9;
  Object.defineProperty(this, 'glo9', {writable: false});

  function read_glo9() { return glo9 }

  %PrepareFunctionForOptimization(read_glo9);
  assertEquals(9, read_glo9());

  // At this point, glo9 has cell type Mutable.

  %OptimizeFunctionOnNextCall(read_glo9);
  assertEquals(9, read_glo9());

  Object.defineProperty(this, 'glo9', {writable: true});
  assertEquals(9, read_glo9());
  assertOptimized(read_glo9);
}
