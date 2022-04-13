// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g1(i) {
  var x = i * 1;
  return (x >>> 0) % 1000000000000;
}

function g2(i) {
  var x = i * 1;
  return ((x >>> 0) % 1000000000000) | 0;
}

function test1() {
  assertEquals(2294967296, g1(-2000000000));
  assertEquals(2294967295, g1(-2000000001));
  assertEquals(2294967290, g1(-2000000006));

  assertEquals(2147483651, g1(-2147483645));
  assertEquals(2147483650, g1(-2147483646));
  assertEquals(2147483649, g1(-2147483647));
  assertEquals(2147483648, g1(-2147483648));
  assertEquals(2147483647, g1(-2147483649));

  assertEquals(3000000000, g1(3000000000));
  assertEquals(3000000001, g1(3000000001));
  assertEquals(3000000002, g1(3000000002));

  assertEquals(4000000000, g1(4000000000));
  assertEquals(4000400001, g1(4000400001));
  assertEquals(4000400002, g1(4000400002));

  assertEquals(3, g1(4294967299));
  assertEquals(2, g1(4294967298));
  assertEquals(1, g1(4294967297));
  assertEquals(0, g1(4294967296));
  assertEquals(4294967295, g1(4294967295));
  assertEquals(4294967294, g1(4294967294));
  assertEquals(4294967293, g1(4294967293));
  assertEquals(4294967292, g1(4294967292));
}

%NeverOptimizeFunction(test1);
test1();

function test2() {
  assertEquals(-2000000000, g2(-2000000000));
  assertEquals(-2000000001, g2(-2000000001));
  assertEquals(-2000000006, g2(-2000000006));

  assertEquals(-2147483645, g2(-2147483645));
  assertEquals(-2147483646, g2(-2147483646));
  assertEquals(-2147483647, g2(-2147483647));
  assertEquals(-2147483648, g2(-2147483648));
  assertEquals(2147483647, g2(-2147483649));

  assertEquals(-1294967296, g2(3000000000));
  assertEquals(-1294967295, g2(3000000001));
  assertEquals(-1294967294, g2(3000000002));

  assertEquals(-294967296, g2(4000000000));
  assertEquals(-294567295, g2(4000400001));
  assertEquals(-294567294, g2(4000400002));

  assertEquals(3, g2(4294967299));
  assertEquals(2, g2(4294967298));
  assertEquals(1, g2(4294967297));
  assertEquals(0, g2(4294967296));
  assertEquals(-1, g2(4294967295));
  assertEquals(-2, g2(4294967294));
  assertEquals(-3, g2(4294967293));
  assertEquals(-4, g2(4294967292));
}

%NeverOptimizeFunction(test2);
test2();
