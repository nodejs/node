// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-bigint

'use strict'

const minus_one = BigInt(-1);
const zero = BigInt(0);
const another_zero = BigInt(0);
const one = BigInt(1);
const another_one = BigInt(1);
const two = BigInt(2);
const three = BigInt(3);
const six = BigInt(6);


// Strict equality
{
  assertTrue(zero === zero);
  assertFalse(zero !== zero);

  assertTrue(zero === another_zero);
  assertFalse(zero !== another_zero);

  assertFalse(zero === one);
  assertTrue(zero !== one);
  assertTrue(one !== zero);
  assertFalse(one === zero);

  assertFalse(zero === 0);
  assertTrue(zero !== 0);
  assertFalse(0 === zero);
  assertTrue(0 !== zero);
}{
  assertTrue(%StrictEqual(zero, zero));
  assertFalse(%StrictNotEqual(zero, zero));

  assertTrue(%StrictEqual(zero, another_zero));
  assertFalse(%StrictNotEqual(zero, another_zero));

  assertFalse(%StrictEqual(zero, one));
  assertTrue(%StrictNotEqual(zero, one));
  assertTrue(%StrictNotEqual(one, zero));
  assertFalse(%StrictEqual(one, zero));

  assertFalse(%StrictEqual(zero, 0));
  assertTrue(%StrictNotEqual(zero, 0));
  assertFalse(%StrictEqual(0, zero));
  assertTrue(%StrictNotEqual(0, zero));
}

// Abstract equality
{
  assertTrue(%Equal(zero, zero));
  assertTrue(%Equal(zero, another_zero));
  assertFalse(%Equal(zero, one));
  assertFalse(%Equal(one, zero));

  assertTrue(%Equal(zero, +0));
  assertTrue(%Equal(zero, -0));
  assertTrue(%Equal(+0, zero));
  assertTrue(%Equal(-0, zero));

  assertTrue(%Equal(zero, false));
  assertTrue(%Equal(one, true));
  assertFalse(%Equal(zero, true));
  assertFalse(%Equal(one, false));
  assertTrue(%Equal(false, zero));
  assertTrue(%Equal(true, one));
  assertFalse(%Equal(true, zero));
  assertFalse(%Equal(false, one));

  assertTrue(%Equal(one, 1));
  assertTrue(%Equal(one, Number(1)));
  assertTrue(%Equal(1, one));
  assertTrue(%Equal(Number(1), one));

  assertTrue(%Equal(minus_one, -1));
  assertTrue(%Equal(minus_one, Number(-1)));
  assertTrue(%Equal(-1, minus_one));
  assertTrue(%Equal(Number(-1), minus_one));

  assertFalse(%Equal(one, -1));
  assertFalse(%Equal(one, Number(-1)));
  assertFalse(%Equal(-1, one));
  assertFalse(%Equal(Number(-1), one));

  assertFalse(%Equal(one, 1.0000000000001));
  assertFalse(%Equal(1.0000000000001, one));

  assertTrue(%Equal(zero, ""));
  assertTrue(%Equal("", zero));
  assertTrue(%Equal(one, "1"));
  assertTrue(%Equal("1", one));
  assertFalse(%Equal(one, "a"));
  assertFalse(%Equal("a", one));

  assertTrue(%Equal(one, {valueOf() { return true }}));
  assertTrue(%Equal({valueOf() { return true }}, one));
  assertFalse(%Equal(two, {valueOf() { return true }}));
  assertFalse(%Equal({valueOf() { return true }}, two));

  assertFalse(%Equal(Symbol(), zero));
  assertFalse(%Equal(zero, Symbol()));
}{
  assertTrue(zero == zero);
  assertTrue(zero == another_zero);
  assertFalse(zero == one);
  assertFalse(one == zero);

  assertTrue(zero == +0);
  assertTrue(zero == -0);
  assertTrue(+0 == zero);
  assertTrue(-0 == zero);

  assertTrue(zero == false);
  assertTrue(one == true);
  assertFalse(zero == true);
  assertFalse(one == false);
  assertTrue(false == zero);
  assertTrue(true == one);
  assertFalse(true == zero);
  assertFalse(false == one);

  assertTrue(one == 1);
  assertTrue(one == Number(1));
  assertTrue(1 == one);
  assertTrue(Number(1) == one);

  assertTrue(minus_one == -1);
  assertTrue(minus_one == Number(-1));
  assertTrue(-1 == minus_one);
  assertTrue(Number(-1) == minus_one);

  assertFalse(one == -1);
  assertFalse(one == Number(-1));
  assertFalse(-1 == one);
  assertFalse(Number(-1) == one);

  assertFalse(one == 1.0000000000001);
  assertFalse(1.0000000000001 == one);

  assertTrue(zero == "");
  assertTrue("" == zero);
  assertTrue(zero == " \t\r\n");
  assertTrue(one == "1");
  assertTrue("1" == one);
  assertFalse(" \t\r\n" == one);
  assertFalse(one == "a");
  assertFalse("a" == one);

  assertTrue(zero == "00000000000000" + "0");

  assertTrue(one == {valueOf() { return true }});
  assertTrue({valueOf() { return true }} == one);
  assertFalse(two == {valueOf() { return true }});
  assertFalse({valueOf() { return true }} == two);

  assertFalse(Symbol() == zero);
  assertFalse(zero == Symbol());

  assertTrue(one == "0b1");
  assertTrue(" 0b1" == one);
  assertTrue(one == "0o1");
  assertTrue("0o1 " == one);
  assertTrue(one == "\n0x1");
  assertTrue("0x1" == one);

  assertFalse(one == "1j");
  assertFalse(one == "0b1ju");
  assertFalse(one == "0o1jun");
  assertFalse(one == "0x1junk");
}{
  assertFalse(%NotEqual(zero, zero));
  assertFalse(%NotEqual(zero, another_zero));
  assertTrue(%NotEqual(zero, one));
  assertTrue(%NotEqual(one, zero));

  assertFalse(%NotEqual(zero, +0));
  assertFalse(%NotEqual(zero, -0));
  assertFalse(%NotEqual(+0, zero));
  assertFalse(%NotEqual(-0, zero));

  assertFalse(%NotEqual(zero, false));
  assertFalse(%NotEqual(one, true));
  assertTrue(%NotEqual(zero, true));
  assertTrue(%NotEqual(one, false));
  assertFalse(%NotEqual(false, zero));
  assertFalse(%NotEqual(true, one));
  assertTrue(%NotEqual(true, zero));
  assertTrue(%NotEqual(false, one));

  assertFalse(%NotEqual(one, 1));
  assertFalse(%NotEqual(one, Number(1)));
  assertFalse(%NotEqual(1, one));
  assertFalse(%NotEqual(Number(1), one));

  assertFalse(%NotEqual(minus_one, -1));
  assertFalse(%NotEqual(minus_one, Number(-1)));
  assertFalse(%NotEqual(-1, minus_one));
  assertFalse(%NotEqual(Number(-1), minus_one));

  assertTrue(%NotEqual(one, -1));
  assertTrue(%NotEqual(one, Number(-1)));
  assertTrue(%NotEqual(-1, one));
  assertTrue(%NotEqual(Number(-1), one));

  assertTrue(%NotEqual(one, 1.0000000000001));
  assertTrue(%NotEqual(1.0000000000001, one));

  assertFalse(%NotEqual(zero, ""));
  assertFalse(%NotEqual("", zero));
  assertFalse(%NotEqual(one, "1"));
  assertFalse(%NotEqual("1", one));
  assertTrue(%NotEqual(one, "a"));
  assertTrue(%NotEqual("a", one));

  assertFalse(%NotEqual(one, {valueOf() { return true }}));
  assertFalse(%NotEqual({valueOf() { return true }}, one));
  assertTrue(%NotEqual(two, {valueOf() { return true }}));
  assertTrue(%NotEqual({valueOf() { return true }}, two));

  assertTrue(%NotEqual(Symbol(), zero));
  assertTrue(%NotEqual(zero, Symbol()));
}{
  assertFalse(zero != zero);
  assertFalse(zero != another_zero);
  assertTrue(zero != one);
  assertTrue(one != zero);

  assertFalse(zero != +0);
  assertFalse(zero != -0);
  assertFalse(+0 != zero);
  assertFalse(-0 != zero);

  assertFalse(zero != false);
  assertFalse(one != true);
  assertTrue(zero != true);
  assertTrue(one != false);
  assertFalse(false != zero);
  assertFalse(true != one);
  assertTrue(true != zero);
  assertTrue(false != one);

  assertFalse(one != 1);
  assertFalse(one != Number(1));
  assertFalse(1 != one);
  assertFalse(Number(1) != one);

  assertFalse(minus_one != -1);
  assertFalse(minus_one != Number(-1));
  assertFalse(-1 != minus_one);
  assertFalse(Number(-1) != minus_one);

  assertTrue(one != -1);
  assertTrue(one != Number(-1));
  assertTrue(-1 != one);
  assertTrue(Number(-1) != one);

  assertTrue(one != 1.0000000000001);
  assertTrue(1.0000000000001 != one);

  assertFalse(zero != "");
  assertFalse("" != zero);
  assertFalse(one != "1");
  assertFalse("1" != one);
  assertTrue(one != "a");
  assertTrue("a" != one);

  assertFalse(one != {valueOf() { return true }});
  assertFalse({valueOf() { return true }} != one);
  assertTrue(two != {valueOf() { return true }});
  assertTrue({valueOf() { return true }} != two);

  assertTrue(Symbol() != zero);
  assertTrue(zero != Symbol());
}

// SameValue
{
  assertTrue(Object.is(zero, zero));
  assertTrue(Object.is(zero, another_zero));
  assertTrue(Object.is(one, one));
  assertTrue(Object.is(one, another_one));
  assertFalse(Object.is(zero, +0));
  assertFalse(Object.is(zero, -0));
  assertFalse(Object.is(+0, zero));
  assertFalse(Object.is(-0, zero));
  assertFalse(Object.is(zero, one));
  assertFalse(Object.is(one, minus_one));
}{
  const obj = Object.defineProperty({}, 'foo',
      {value: zero, writable: false, configurable: false});

  assertTrue(Reflect.defineProperty(obj, 'foo', {value: zero}));
  assertTrue(Reflect.defineProperty(obj, 'foo', {value: another_zero}));
  assertFalse(Reflect.defineProperty(obj, 'foo', {value: one}));
}{
  assertTrue(%SameValue(zero, zero));
  assertTrue(%SameValue(zero, another_zero));

  assertFalse(%SameValue(zero, +0));
  assertFalse(%SameValue(zero, -0));

  assertFalse(%SameValue(+0, zero));
  assertFalse(%SameValue(-0, zero));

  assertTrue(%SameValue(one, one));
  assertTrue(%SameValue(one, another_one));
}

// SameValueZero
{
  assertTrue([zero].includes(zero));
  assertTrue([zero].includes(another_zero));

  assertFalse([zero].includes(+0));
  assertFalse([zero].includes(-0));

  assertFalse([+0].includes(zero));
  assertFalse([-0].includes(zero));

  assertTrue([one].includes(one));
  assertTrue([one].includes(another_one));

  assertFalse([one].includes(1));
  assertFalse([1].includes(one));
}{
  assertTrue(new Set([zero]).has(zero));
  assertTrue(new Set([zero]).has(another_zero));

  assertFalse(new Set([zero]).has(+0));
  assertFalse(new Set([zero]).has(-0));

  assertFalse(new Set([+0]).has(zero));
  assertFalse(new Set([-0]).has(zero));

  assertTrue(new Set([one]).has(one));
  assertTrue(new Set([one]).has(another_one));
}{
  assertTrue(new Map([[zero, 42]]).has(zero));
  assertTrue(new Map([[zero, 42]]).has(another_zero));

  assertFalse(new Map([[zero, 42]]).has(+0));
  assertFalse(new Map([[zero, 42]]).has(-0));

  assertFalse(new Map([[+0, 42]]).has(zero));
  assertFalse(new Map([[-0, 42]]).has(zero));

  assertTrue(new Map([[one, 42]]).has(one));
  assertTrue(new Map([[one, 42]]).has(another_one));
}{
  assertTrue(%SameValueZero(zero, zero));
  assertTrue(%SameValueZero(zero, another_zero));

  assertFalse(%SameValueZero(zero, +0));
  assertFalse(%SameValueZero(zero, -0));

  assertFalse(%SameValueZero(+0, zero));
  assertFalse(%SameValueZero(-0, zero));

  assertTrue(%SameValueZero(one, one));
  assertTrue(%SameValueZero(one, another_one));
}

// Abstract comparison
{
  let undef = Symbol();

  assertTrue(%Equal(zero, zero));
  assertTrue(%GreaterThanOrEqual(zero, zero));

  assertTrue(%LessThan(zero, one));
  assertTrue(%GreaterThan(one, zero));

  assertTrue(%LessThan(minus_one, one));
  assertTrue(%GreaterThan(one, minus_one));

  assertTrue(%Equal(zero, -0));
  assertTrue(%LessThanOrEqual(zero, -0));
  assertTrue(%GreaterThanOrEqual(zero, -0));
  assertTrue(%Equal(-0, zero));
  assertTrue(%LessThanOrEqual(-0, zero));
  assertTrue(%GreaterThanOrEqual(-0, zero));

  assertTrue(%Equal(zero, 0));
  assertTrue(%Equal(0, zero));

  assertTrue(%LessThan(minus_one, 1));
  assertTrue(%GreaterThan(1, minus_one));

  assertFalse(%LessThan(six, NaN));
  assertFalse(%GreaterThan(six, NaN));
  assertFalse(%Equal(six, NaN));
  assertFalse(%LessThan(NaN, six));
  assertFalse(%GreaterThan(NaN, six));
  assertFalse(%Equal(NaN, six));

  assertTrue(%LessThan(six, Infinity));
  assertTrue(%GreaterThan(Infinity, six));

  assertTrue(%GreaterThan(six, -Infinity));
  assertTrue(%LessThan(-Infinity, six));

  assertTrue(%GreaterThan(six, 5.99999999));
  assertTrue(%LessThan(5.99999999, six));

  assertTrue(%Equal(zero, ""));
  assertTrue(%LessThanOrEqual(zero, ""));
  assertTrue(%GreaterThanOrEqual(zero, ""));
  assertTrue(%Equal("", zero));
  assertTrue(%LessThanOrEqual("", zero));
  assertTrue(%GreaterThanOrEqual("", zero));

  assertTrue(%Equal(minus_one, "\t-1 "));
  assertTrue(%LessThanOrEqual(minus_one, "\t-1 "));
  assertTrue(%GreaterThanOrEqual(minus_one, "\t-1 "));
  assertTrue(%Equal("\t-1 ", minus_one));
  assertTrue(%LessThanOrEqual("\t-1 ", minus_one));
  assertTrue(%GreaterThanOrEqual("\t-1 ", minus_one));

  assertFalse(%LessThan(minus_one, "-0x1"));
  assertFalse(%GreaterThan(minus_one, "-0x1"));
  assertFalse(%Equal(minus_one, "-0x1"));
  assertFalse(%LessThan("-0x1", minus_one));
  assertFalse(%GreaterThan("-0x1", minus_one));
  assertFalse(%Equal("-0x1", minus_one));

  const unsafe = "9007199254740993";  // 2**53 + 1
  assertTrue(%GreaterThan(eval(unsafe + "n"), unsafe));
  assertTrue(%LessThan(unsafe, eval(unsafe + "n")));

  assertThrows(() => %LessThan(six, Symbol(6)), TypeError);
  assertThrows(() => %LessThan(Symbol(6), six), TypeError);

  var value_five_string_six = {
      valueOf() { return Object(5); },
      toString() { return 6; }
  };
  assertTrue(%LessThanOrEqual(six, value_five_string_six));
  assertTrue(%GreaterThanOrEqual(six, value_five_string_six));
  assertTrue(%LessThanOrEqual(value_five_string_six, six));
  assertTrue(%GreaterThanOrEqual(value_five_string_six, six));
}{
  assertFalse(zero < zero);
  assertTrue(zero <= zero);

  assertTrue(zero < one);
  assertTrue(zero <= one);
  assertFalse(one < zero);
  assertFalse(one <= zero);

  assertTrue(minus_one < one);
  assertTrue(minus_one <= one);
  assertFalse(one < minus_one);
  assertFalse(one <= minus_one);

  assertFalse(zero < -0);
  assertTrue(zero <= -0);
  assertFalse(-0 < zero);
  assertTrue(-0 <= zero);

  assertFalse(zero < 0);
  assertTrue(zero <= 0);
  assertFalse(0 < zero);
  assertTrue(0 <= zero);

  assertTrue(minus_one < 1);
  assertTrue(minus_one <= 1);
  assertFalse(1 < minus_one);
  assertFalse(1 <= minus_one);

  assertFalse(six < NaN);
  assertFalse(six <= NaN);
  assertFalse(NaN < six);
  assertFalse(NaN <= six);

  assertTrue(six < Infinity);
  assertTrue(six <= Infinity);
  assertFalse(Infinity < six);
  assertFalse(Infinity <= six);

  assertFalse(six < -Infinity);
  assertFalse(six <= -Infinity);
  assertTrue(-Infinity < six);
  assertTrue(-Infinity <= six);

  assertFalse(six < 5.99999999);
  assertFalse(six <= 5.99999999);
  assertTrue(5.99999999 < six);
  assertTrue(5.99999999 <= six);

  assertFalse(zero < "");
  assertTrue(zero <= "");
  assertFalse("" < zero);
  assertTrue("" <= zero);

  assertFalse(minus_one < "\t-1 ");
  assertTrue(minus_one <= "\t-1 ");
  assertFalse("\t-1 " < minus_one);
  assertTrue("\t-1 " <= minus_one);

  assertFalse(minus_one < "-0x1");
  assertFalse(minus_one <= "-0x1");
  assertFalse("-0x1" < minus_one);
  assertFalse("-0x1" <= minus_one);

  const unsafe = "9007199254740993";  // 2**53 + 1
  assertFalse(eval(unsafe + "n") < unsafe);
  assertFalse(eval(unsafe + "n") <= unsafe);
  assertTrue(unsafe < eval(unsafe + "n"));
  assertTrue(unsafe <= eval(unsafe + "n"));

  assertThrows(() => six < Symbol(6), TypeError);
  assertThrows(() => six <= Symbol(6), TypeError);
  assertThrows(() => Symbol(6) < six, TypeError);
  assertThrows(() => Symbol(6) <= six, TypeError);

  assertFalse(six < {valueOf() {return Object(5)}, toString() {return 6}});
  assertTrue(six <= {valueOf() {return Object(5)}, toString() {return 6}});
  assertFalse({valueOf() {return Object(5)}, toString() {return 6}} < six);
  assertTrue({valueOf() {return Object(5)}, toString() {return 6}} <= six);
}
