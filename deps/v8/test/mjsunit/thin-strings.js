// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function get_thin_string(a, b) {
  var str = a + b;  // Make a ConsString.
  var o = {};
  o[str];  // Turn it into a ThinString.
  return str;
}

var str = get_thin_string("foo", "bar");

var re = /.o+ba./;
assertEquals(["foobar"], re.exec(str));
assertEquals(["foobar"], re.exec(str));
assertEquals(["foobar"], re.exec(str));

function CheckCS() {
  assertEquals("o", str.substring(1, 2));
  assertEquals("f".charCodeAt(0), str.charCodeAt(0));
  assertEquals("f", str.split(/oo/)[0]);
};
%PrepareFunctionForOptimization(CheckCS);
CheckCS();
%OptimizeFunctionOnNextCall(CheckCS);
CheckCS();

function CheckTF() {
  try {
  } catch (e) {
  }  // Turbofan.
  assertEquals("o", str.substring(1, 2));
  assertEquals("f".charCodeAt(0), str.charCodeAt(0));
  assertEquals("f", str.split(/oo/)[0]);
};
%PrepareFunctionForOptimization(CheckTF);
CheckTF();
%OptimizeFunctionOnNextCall(CheckTF);
CheckTF();

// Flat cons strings can point to a thin string.

function get_cons_thin_string(a, b) {
  // Make a ConsString.
  var s = a + b;
  // Flatten it.
  s.endsWith("a");
  // Internalize the first part.
  var o = {};
  o[s];
  return s;
}

var s = get_cons_thin_string("__________", "@@@@@@@@@@a");
s.match(/.*a/);
assertEquals("________", s.substring(0, 8));

function cc1(s) {
  assertEquals(95, s.charCodeAt(0));
  assertEquals(95, s.codePointAt(0));
};
%PrepareFunctionForOptimization(cc1);
cc1(s);
cc1(s);
%OptimizeFunctionOnNextCall(cc1);
cc1(s);

// Sliced strings can have a thin string as their parent.

function get_sliced_thin_string(a, b) {
  // Make a long thin string.
  var s = a + b;
  // Slice a substring out of it.
  var slice = s.substring(0, 20);
  // Make the original string thin.
  var o = {};
  o[s];
  return slice;
}

var t = get_sliced_thin_string(
    'abcdefghijklmnopqrstuvwxyz', 'abcdefghijklmnopqrstuvwxyz');
assertEquals("abcdefghijklmnopqrst", decodeURI(t));

function cc2(s) {
  assertEquals(97, s.charCodeAt(0));
  assertEquals(97, s.codePointAt(0));
};
%PrepareFunctionForOptimization(cc2);
cc2(t);
cc2(t);
%OptimizeFunctionOnNextCall(cc2);
cc2(t);

function string_table_lookup_sliced_thin_string(a, b) {
  // Make a ConsString.
  var s = a + b;
  // Slice a substring out of it.
  var slice = s.substring(0, 20);
  // Make the original string thin.
  var o = {};
  o[s];
  // Try to internalize the SlicedString.
  o[slice];
}

string_table_lookup_sliced_thin_string(
    'abcdefghijklmnopqrstuvwxyz', '0123456789');
