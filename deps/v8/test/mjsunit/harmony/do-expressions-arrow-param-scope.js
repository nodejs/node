// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-do-expressions

(function testBasic() {
  let f = (x = eval("var z = 42; z"), y = do { 43 }) => x + y;
  assertEquals(85, f());
})();

(function testReturnParam() {
  let f = (x = eval("var z = 42; z"), y = do { x }) => x + y;
  assertEquals(84, f());
})();

(function testCaptureParam() {
  let f = (x = eval("var z = 42; z"), y = do { () => x }) => x + y();
  assertEquals(84, f());
})();

(function testScoped() {
  let f = (x = eval("var z = 42; z"), y = do { let z; x }) => x + y;
  assertEquals(84, f());
})();

(function testCaptureScoped() {
  let f = (x = eval("var z = 42; z"), y = do { let z; () => x }) => x + y();
  assertEquals(84, f());
})();

(function testCaptureOuter() {
  let z = 44;
  let f = (x = eval("var z = 42; z"), y = do { () => z }) => x + y();
  assertEquals(86, f())
})();

(function testCaptureOuterScoped() {
  let z = 44;
  let f = (x = eval("var z = 42; z"), y = do { let q; () => z }) => x + y();
  assertEquals(86, f())
})();

(function testWith() {
  let f = (x = eval("var z = 42; z"),
           y = do {
             with ({foo: "bar"}) {
               () => x }
           }) => x + y();
  assertEquals(84, f())
})();

(function testTry() {
  let f = (x = eval("var z = 42; z"),
           y = do {
             try { () => x }
             catch (e) { }
           }) => x + y();
  assertEquals(84, f())
})();

(function testCatch() {
  let f = (x = eval("var z = 42; z"),
           y = do {
             try { throw 42 }
             catch (e) { () => x }
           }) => x + y();
  assertEquals(84, f())
})();

(function testFinally() {
  let z = 44;
  let q;
  let f = (x = eval("var z = 42; z"),
           y = do {
             try { }
             catch (e) { }
             finally { q = () => z }
             q;
           }) => x + y();
  assertEquals(86, f())
})();

(function testFinallyThrow() {
  let z = 44;
  let q;
  let f = (x = eval("var z = 42; z"),
           y = do {
             try { throw 42; }
             catch (e) { }
             finally { q = () => z }
             q;
           }) => x + y();
  assertEquals(86, f())
})();
