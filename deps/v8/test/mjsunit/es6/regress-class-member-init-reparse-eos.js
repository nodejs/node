// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let x = 1;

// Instance field initializer throwing when the final uninitialized field is
// named static, get, set, or async (with or without semicolon / static).
assertThrows(() => {
  class C { a = x(); static; }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); static }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); get; }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); get }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); set; }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); set }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); async; }
  new C();
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { a = x(); async }
  new C();
}, TypeError, "x is not a function");

// Static field initializer throwing when the final uninitialized field is
// named static, get, set, or async (instance or static).
assertThrows(() => {
  class C { static a = x(); static; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); get; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); set; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); async; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static static; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static get; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static get }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static set; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static set }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static async; }
}, TypeError, "x is not a function");

assertThrows(() => {
  class C { static a = x(); static async }
}, TypeError, "x is not a function");

// Incomplete classes at EOF must still throw SyntaxError.
assertThrows(() => eval("class C { static"), SyntaxError);
assertThrows(() => eval("class C { get"), SyntaxError);
assertThrows(() => eval("class C { set"), SyntaxError);
assertThrows(() => eval("class C { async"), SyntaxError);
assertThrows(() => eval("class C { static get"), SyntaxError);
assertThrows(() => eval("class C { static set"), SyntaxError);
assertThrows(() => eval("class C { static async"), SyntaxError);
