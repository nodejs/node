// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

var shadowRealm = new ShadowRealm();

// re-throwing with SyntaxError
assertThrows(() => shadowRealm.evaluate('...'), SyntaxError, 'Unexpected end of input')

// builtin
var wrapped = shadowRealm.evaluate('String.prototype.substring');
assertEquals(wrapped.call('123', 1), '23');

// bound function
var wrapped = shadowRealm.evaluate('(function it() { return this.a }).bind({ a: 1 })');
assertEquals(wrapped(), 1);

// nested bound function
var wrapped = shadowRealm.evaluate('(function it() { return this.a }).bind({ a: 1 }).bind().bind()');
assertEquals(wrapped(), 1);

// function with function context
var wrapped = shadowRealm.evaluate(`
(function () {
  var a = 1;
  function it() { return a++; };
  return it;
})()
`);
assertEquals(wrapped(), 1);
assertEquals(wrapped(), 2);

// callable proxy
var wrapped = shadowRealm.evaluate('new Proxy(() => 1, {})');
assertEquals(wrapped(), 1);

// nested callable proxy
var wrapped = shadowRealm.evaluate('new Proxy(new Proxy(new Proxy(() => 1, {}), {}), {})');
assertEquals(wrapped(), 1);

// revocable proxy
var wrapped = shadowRealm.evaluate(`
var revocable = Proxy.revocable(() => 1, {});
globalThis.revoke = () => {
  revocable.revoke();
};

revocable.proxy;
`);
var revoke = shadowRealm.evaluate('globalThis.revoke');
assertEquals(wrapped(), 1);
revoke();
assertThrows(() => wrapped(), TypeError, "Cannot perform 'apply' on a proxy that has been revoked");

// revoked proxy
assertThrows(() => shadowRealm.evaluate(`
var revocable = Proxy.revocable(() => 1, {});
revocable.revoke();
revocable.proxy;
`), TypeError, "Cannot wrap target callable");
