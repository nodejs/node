// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyError {};
const throwing = {toString() {throw new MyError}};
const empties = ['', {toString() {return ''}}];

{
  const s = '';

  assertThrows(_ => s.padStart(Symbol(), throwing), TypeError);
  assertEquals(s, s.padStart(NaN, throwing));
  assertEquals(s, s.padStart(-Infinity, throwing));
  assertEquals(s, s.padStart(-9, throwing));
  assertEquals(s, s.padStart(-1, throwing));
  assertEquals(s, s.padStart(-0, throwing));
  assertEquals(s, s.padStart(0, throwing));
  assertThrows(_ => s.padStart(3, throwing), MyError);
  assertThrows(_ => s.padStart(9, throwing), MyError);
  assertThrows(_ => s.padStart(2**31-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**31, throwing), MyError);
  assertThrows(_ => s.padStart(2**32-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**32, throwing), MyError);
  assertThrows(_ => s.padStart(2**53-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**53, throwing), MyError);
  assertThrows(_ => s.padStart(Infinity, throwing), MyError);

  assertThrows(_ => s.padEnd(Symbol(), throwing), TypeError);
  assertEquals(s, s.padEnd(NaN, throwing));
  assertEquals(s, s.padEnd(-Infinity, throwing));
  assertEquals(s, s.padEnd(-9, throwing));
  assertEquals(s, s.padEnd(-1, throwing));
  assertEquals(s, s.padEnd(-0, throwing));
  assertEquals(s, s.padEnd(0, throwing));
  assertThrows(_ => s.padEnd(3, throwing), MyError);
  assertThrows(_ => s.padEnd(9, throwing), MyError);
  assertThrows(_ => s.padEnd(2**31-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**31, throwing), MyError);
  assertThrows(_ => s.padEnd(2**32-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**32, throwing), MyError);
  assertThrows(_ => s.padEnd(2**53-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**53, throwing), MyError);
  assertThrows(_ => s.padEnd(Infinity, throwing), MyError);

  for (const empty of empties) {
    assertThrows(_ => s.padStart(Symbol(), empty), TypeError);
    assertEquals(s, s.padStart(NaN, empty));
    assertEquals(s, s.padStart(-Infinity, empty));
    assertEquals(s, s.padStart(-9, empty));
    assertEquals(s, s.padStart(-1, empty));
    assertEquals(s, s.padStart(-0, empty));
    assertEquals(s, s.padStart(0, empty));
    assertEquals(s, s.padStart(3, empty));
    assertEquals(s, s.padStart(9, empty));
    assertEquals(s, s.padStart(2**31-1, empty));
    assertEquals(s, s.padStart(2**31, empty));
    assertEquals(s, s.padStart(2**32-1, empty));
    assertEquals(s, s.padStart(2**32, empty));
    assertEquals(s, s.padStart(2**53-1, empty));
    assertEquals(s, s.padStart(2**53, empty));
    assertEquals(s, s.padStart(Infinity, empty));

    assertThrows(_ => s.padEnd(Symbol(), empty), TypeError);
    assertEquals(s, s.padEnd(NaN, empty));
    assertEquals(s, s.padEnd(-Infinity, empty));
    assertEquals(s, s.padEnd(-9, empty));
    assertEquals(s, s.padEnd(-1, empty));
    assertEquals(s, s.padEnd(-0, empty));
    assertEquals(s, s.padEnd(0, empty));
    assertEquals(s, s.padEnd(3, empty));
    assertEquals(s, s.padEnd(9, empty));
    assertEquals(s, s.padEnd(2**31-1, empty));
    assertEquals(s, s.padEnd(2**31, empty));
    assertEquals(s, s.padEnd(2**32-1, empty));
    assertEquals(s, s.padEnd(2**32, empty));
    assertEquals(s, s.padEnd(2**53-1, empty));
    assertEquals(s, s.padEnd(2**53, empty));
    assertEquals(s, s.padEnd(Infinity, empty));
  }
}

{
  const s = 'hello';

  assertThrows(_ => s.padStart(Symbol(), throwing), TypeError);
  assertEquals(s, s.padStart(NaN, throwing));
  assertEquals(s, s.padStart(-Infinity, throwing));
  assertEquals(s, s.padStart(-9, throwing));
  assertEquals(s, s.padStart(-1, throwing));
  assertEquals(s, s.padStart(-0, throwing));
  assertEquals(s, s.padStart(0, throwing));
  assertEquals(s, s.padStart(3, throwing));
  assertThrows(_ => s.padStart(9, throwing), MyError);
  assertThrows(_ => s.padStart(2**31-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**31, throwing), MyError);
  assertThrows(_ => s.padStart(2**32-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**32, throwing), MyError);
  assertThrows(_ => s.padStart(2**53-1, throwing), MyError);
  assertThrows(_ => s.padStart(2**53, throwing), MyError);
  assertThrows(_ => s.padStart(Infinity, throwing), MyError);

  assertThrows(_ => s.padEnd(Symbol(), throwing), TypeError);
  assertEquals(s, s.padEnd(NaN, throwing));
  assertEquals(s, s.padEnd(-Infinity, throwing));
  assertEquals(s, s.padEnd(-9, throwing));
  assertEquals(s, s.padEnd(-1, throwing));
  assertEquals(s, s.padEnd(-0, throwing));
  assertEquals(s, s.padEnd(0, throwing));
  assertEquals(s, s.padEnd(3, throwing));
  assertThrows(_ => s.padEnd(9, throwing), MyError);
  assertThrows(_ => s.padEnd(2**31-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**31, throwing), MyError);
  assertThrows(_ => s.padEnd(2**32-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**32, throwing), MyError);
  assertThrows(_ => s.padEnd(2**53-1, throwing), MyError);
  assertThrows(_ => s.padEnd(2**53, throwing), MyError);
  assertThrows(_ => s.padEnd(Infinity, throwing), MyError);

  for (const empty of empties) {
    assertThrows(_ => s.padStart(Symbol(), empty), TypeError);
    assertEquals(s, s.padStart(NaN, empty));
    assertEquals(s, s.padStart(-Infinity, empty));
    assertEquals(s, s.padStart(-9, empty));
    assertEquals(s, s.padStart(-1, empty));
    assertEquals(s, s.padStart(-0, empty));
    assertEquals(s, s.padStart(0, empty));
    assertEquals(s, s.padStart(3, empty));
    assertEquals(s, s.padStart(9, empty));
    assertEquals(s, s.padStart(2**31-1, empty));
    assertEquals(s, s.padStart(2**31, empty));
    assertEquals(s, s.padStart(2**32-1, empty));
    assertEquals(s, s.padStart(2**32, empty));
    assertEquals(s, s.padStart(2**53-1, empty));
    assertEquals(s, s.padStart(2**53, empty));
    assertEquals(s, s.padStart(Infinity, empty));

    assertThrows(_ => s.padEnd(Symbol(), empty), TypeError);
    assertEquals(s, s.padEnd(NaN, empty));
    assertEquals(s, s.padEnd(-Infinity, empty));
    assertEquals(s, s.padEnd(-9, empty));
    assertEquals(s, s.padEnd(-1, empty));
    assertEquals(s, s.padEnd(-0, empty));
    assertEquals(s, s.padEnd(0, empty));
    assertEquals(s, s.padEnd(3, empty));
    assertEquals(s, s.padEnd(9, empty));
    assertEquals(s, s.padEnd(2**31-1, empty));
    assertEquals(s, s.padEnd(2**31, empty));
    assertEquals(s, s.padEnd(2**32-1, empty));
    assertEquals(s, s.padEnd(2**32, empty));
    assertEquals(s, s.padEnd(2**53-1, empty));
    assertEquals(s, s.padEnd(2**53, empty));
    assertEquals(s, s.padEnd(Infinity, empty));
  }
}
