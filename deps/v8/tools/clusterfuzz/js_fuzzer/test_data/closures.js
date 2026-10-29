// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = 0;
let b = 0;

// ----------------------------------------------------------------------------
// Closures that can be transformed.

// Basic cases.
(() => {
  const b = 1;
  foo(a + b);
})();

(() => {
  const a = 1;
  for (let c = 0; c < a; c++) foo(a + b + c + d);
})();

(function bar() {
  const b = 1;
  for (let c = 0; c < a; c++) foo(a + b + c + d);
})();

(function bar() {
  const a = 1;
  for (let c = 0; c < a; c++) foo(a + b + c + d);
})();

(function () {
  const b = 1;
  for (let c = 0; c < a; c++) foo(a + b + c + d);
  this.a = 6;
})();

(function () {
  const a = 1;
  for (let c = 0; c < a; c++) foo(a + b + c + d);
})();

// Various name collision examples.
(function () {
  const col1 = 1;
})();

const col1 = 2;

(function () {
  var col2 = 1;
})();

col2 = 2;

var col3 = 2;

(function () {
  var col3 = 1;
})();

var col4 = 2;

(function () {
  col4 = 1;
})();

// Nested.
(() => {
  (() => {
    let a = 1;
    while (b) b += a;
  })();
})();

// Nested functions that return.
(() => {
  (() => {
    let a = 1;
    return a;
  })();

  (() => {
    if(foo()) return 0;
  })();
})();

// Removable outer closure, but inner closures that stress the function stack.
(() => {
  (function bar(a=() => { return 0; }) {
    const b = () => { return 0; };
  })(() => { return 0; });
  (() => {
    return () => { return 0; };
  })();
})();

// ----------------------------------------------------------------------------
// Closures that can't be transformed.

// Don't bother with empty bodies.
(function () {})();

// Self reference.
(function bar() {
  const a = bar();
})();

// Return statements.
(function () {
  if (b++ > 10) return;
  console.log(42);
})();

(function () {
  const b = 0;
  return b;
})();

// No block body.
(() => console.log(42))();

// Other non-supported cases.
(async function () {
  console.log(42);
})();

(function* () {
  console.log(42);
})();

// Normal function definition.
function foo2() {
  let a = 0;
}

// Function expression (but no call).
const f = function foo3() {
  const b = 0;
};

// Not top-level scope.
{
  (() => { const a = 1; })();
}

function foo3() {
  (() => { const a = 1; })();
}

(function () {
  (() => { console.log(42); })();
  (() => { console.log(42); })();
  if (foo()) return;
})();

// Params.
(() => {
  console.log(x);
})(5);

(function bar() {
  console.log(x);
})(5);

// Arguments.
((x) => {
  console.log(x);
})();

(function bar(x) {
  console.log(x);
})();

// Call expressions with other types of callees.
foo();
foo[(() => { foo(); })()]();
[() => { foo(); }][0]();

// Sequence.
(() => { foo(); })(), (() => { foo(); })();
