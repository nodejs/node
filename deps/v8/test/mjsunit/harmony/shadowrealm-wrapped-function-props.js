// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm --allow-natives-syntax

// Test wrapped function returned from ShadowRealm.prototype.evaluate
function shadowRealmEvaluate(sourceText) {
  var shadowRealm = new ShadowRealm();
  shadowRealm.evaluate(`function makeSlow(o) {
    for (var i = 0; i < 1200; i++) {
      o["o"+i] = 0;
    }
    if (%HasFastProperties(o)) {
      throw new Error('o should be slow');
    }
    return o;
  }`);
  return shadowRealm.evaluate(sourceText);
}

// Test wrapped function returned from WrappedFunction.[[Call]]
function wrappedFunctionEvaluate(sourceText) {
  var shadowRealm = new ShadowRealm();
  shadowRealm.evaluate(`function makeSlow(o) {
    for (var i = 0; i < 1200; i++) {
      o["o"+i] = 0;
    }
    if (%HasFastProperties(o)) {
      throw new Error('o should be slow');
    }
    return o;
  }`);
  // Create a wrapped function from sourceText in the shadow realm and return it.
  return shadowRealm.evaluate('text => eval(text)')(sourceText);
}

suite(shadowRealmEvaluate);
suite(wrappedFunctionEvaluate);

function suite(evaluate) {
  // function
  var wrapped = evaluate('function foo() {}; foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 0);
  // The properties should be accessor infos.
  assertTrue(%HasFastProperties(wrapped));

  var wrapped = evaluate('function foo(bar) {}; foo');
  assertEquals(wrapped.length, 1);

  // builtin function
  var wrapped = evaluate('String.prototype.substring');
  assertEquals(wrapped.name, 'substring');
  assertEquals(wrapped.length, 2);

  // callable proxy
  var wrapped = evaluate('new Proxy(function foo(arg) {}, {})');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 1);

  // nested callable proxy
  var wrapped = evaluate('new Proxy(new Proxy(new Proxy(function foo(arg) {}, {}), {}), {})');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 1);

  // bound function
  var wrapped = evaluate('(function foo(arg) { return this.a }).bind({ a: 1 })');
  assertEquals(wrapped.name, 'bound foo');
  assertEquals(wrapped.length, 1);

  // nested bound function
  var wrapped = evaluate('(function foo(arg) { return this.a }).bind({ a: 1 }).bind().bind()');
  assertEquals(wrapped.name, 'bound bound bound foo');
  assertEquals(wrapped.length, 1);

  // bound function with args
  var wrapped = evaluate('(function foo(arg1, arg2) { return this.a }).bind({ a: 1 }, 1)');
  assertEquals(wrapped.name, 'bound foo');
  assertEquals(wrapped.length, 1);

  // function with length modified
  var wrapped = evaluate('function foo(arg) {}; Object.defineProperty(foo, "length", {value: 123}); foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 123);

  var wrapped = evaluate('function foo(arg) {}; Object.defineProperty(foo, "length", {value: "123"}); foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 0);

  var wrapped = evaluate('function foo(arg) {}; delete foo.length; foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 0);

  var wrapped = evaluate('function foo(arg) {}; Object.defineProperty(foo, "length", {value: 123}); makeSlow(foo)');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 123);

  // function with name modified
  var wrapped = evaluate('function foo(arg) {}; Object.defineProperty(foo, "name", {value: "bar"}); foo');
  assertEquals(wrapped.name, 'bar');
  assertEquals(wrapped.length, 1);

  var wrapped = evaluate('function foo(arg) {}; Object.defineProperty(foo, "name", {value: new String("bar")}); foo');
  assertEquals(wrapped.name, '');
  assertEquals(wrapped.length, 1);

  var wrapped = evaluate('function foo(arg) {}; delete foo.name; foo');
  assertEquals(wrapped.name, '');
  assertEquals(wrapped.length, 1);

  // function with prototype modified
  var wrapped = evaluate('function foo(arg) {}; Object.setPrototypeOf(foo, Object); foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 1);

  // function with additional properties
  var wrapped = evaluate('function foo(arg) {}; foo.bar = 123; foo');
  assertEquals(wrapped.name, 'foo');
  assertEquals(wrapped.length, 1);
}
