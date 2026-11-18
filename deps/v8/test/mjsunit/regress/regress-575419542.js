// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a = eval("{ function arguments() {} }")) {
  return typeof arguments;
}
assertEquals('object', f());

function h(a = eval("{ function arguments() {} }; typeof arguments")) {
  return a;
}
assertEquals('object', h());

function g(a = eval("var inner; { inner = typeof arguments; function arguments() {} }; inner")) {
  return [a, typeof arguments];
}
assertEquals(['function', 'object'], g());

const arrow = (a = eval("{ function arguments() {} }; typeof arguments")) => a;
assertEquals('function', arrow());

function bodyEval(a = 1) {
  eval("{ function arguments() {} }");
  return typeof arguments;
}
assertEquals('function', bodyEval());

function nested(a = eval("eval('{ function arguments() {} }')")) {
  return typeof arguments;
}
assertEquals('object', nested());

function* gen(a = eval("{ function arguments() {} }")) {
  yield typeof arguments;
}
assertEquals('object', gen().next().value);
