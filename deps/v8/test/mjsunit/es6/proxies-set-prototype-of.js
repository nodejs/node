// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target = { target: 1 };
target.__proto__ = {};
var handler = { handler: 1 };
var proxy = new Proxy(target, handler);

assertSame(Object.getPrototypeOf(proxy), target.__proto__ );


assertThrows(function() { Object.setPrototypeOf(proxy, undefined) }, TypeError);
assertThrows(function() { Object.setPrototypeOf(proxy, 1) }, TypeError);

var prototype = [1];
assertSame(proxy, Object.setPrototypeOf(proxy, prototype));
assertSame(prototype, Object.getPrototypeOf(proxy));
assertSame(prototype, Object.getPrototypeOf(target));

var pair = Proxy.revocable(target, handler);
assertSame(pair.proxy, Object.setPrototypeOf(pair.proxy, prototype));
assertSame(prototype, Object.getPrototypeOf(pair.proxy));
pair.revoke();
assertThrows('Object.setPrototypeOf(pair.proxy, prototype)', TypeError);

handler.setPrototypeOf = function(target, proto) {
  return false;
};
assertThrows(function() { Object.setPrototypeOf(proxy, {a:1}) }, TypeError);

handler.setPrototypeOf = function(target, proto) {
  return undefined;
};
assertThrows(function() { Object.setPrototypeOf(proxy, {a:2}) }, TypeError);

handler.setPrototypeOf = function(proto) {};
assertThrows(function() { Object.setPrototypeOf(proxy, {a:3}) }, TypeError);

handler.setPrototypeOf = function(target, proto) {
  throw Error();
};
assertThrows(function() { Object.setPrototypeOf(proxy, {a:4}) }, Error);

var seen_prototype;
var seen_target;
handler.setPrototypeOf = function(target, proto) {
  seen_target = target;
  seen_prototype = proto;
  return true;
}
assertSame(Object.setPrototypeOf(proxy, {a:5}), proxy);
assertSame(target, seen_target);
assertEquals({a:5}, seen_prototype);

(function setPrototypeProxyTarget() {
  var target = { target: 1 };
  target.__proto__ = {};
  var handler = {};
  var handler2 = {};
  var target2 = new Proxy(target, handler2);
  var proxy2 = new Proxy(target2, handler);
  assertSame(Object.getPrototypeOf(proxy2), target.__proto__ );

  var prototype = [2,3];
  assertSame(proxy2, Object.setPrototypeOf(proxy2, prototype));
  assertSame(prototype, Object.getPrototypeOf(proxy2));
  assertSame(prototype, Object.getPrototypeOf(target));
})();

(function testProxyTrapInconsistent() {
  var target = { target: 1 };
  target.__proto__ = {};
  var handler = {};
  var handler2 = {
  };

  var target2 = new Proxy(target, handler);
  var proxy2 = new Proxy(target2, handler2);

  // If the final target is extensible we can set any prototype.
  var prototype = [1];
  Reflect.setPrototypeOf(proxy2, prototype);
  assertSame(prototype, Reflect.getPrototypeOf(target));

  handler2.setPrototypeOf = function(target, value) {
    Reflect.setPrototypeOf(target, value);
    return true;
  };
  prototype = [2];
  Reflect.setPrototypeOf(proxy2, prototype);
  assertSame(prototype, Reflect.getPrototypeOf(target));

  // Prevent getting the target's prototype used to check the invariant.
  var gotPrototype = false;
  handler.getPrototypeOf = function() {
    gotPrototype = true;
    throw TypeError()
  };
  // If the target is extensible we do not check the invariant.
  prototype = [3];
  Reflect.setPrototypeOf(proxy2, prototype);
  assertFalse(gotPrototype);
  assertSame(prototype, Reflect.getPrototypeOf(target));

  // Changing the prototype of a non-extensible target will trigger the
  // invariant-check and throw in the above handler.
  Reflect.preventExtensions(target);
  assertThrows(() => {Reflect.setPrototypeOf(proxy2, [4])}, TypeError);
  assertTrue(gotPrototype);
  assertEquals([3], Reflect.getPrototypeOf(target));

  // Setting the prototype of a non-extensible target is fine if the prototype
  // doesn't change.
  delete handler.getPrototypeOf;
  Reflect.setPrototypeOf(proxy2, prototype);
  // Changing the prototype will throw.
  prototype = [5];
  assertThrows(() => {Reflect.setPrototypeOf(proxy2, prototype)}, TypeError);
})();

(function testProxyTrapReturnsFalse() {
  var handler = {};
  handler.setPrototypeOf = () => false;
  var target = new Proxy({}, {isExtensible: () => assertUnreachable()});
  var object = new Proxy(target, handler);
  assertFalse(Reflect.setPrototypeOf(object, {}));
})();
