// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testNonConstructable() {
  var proxy = new Proxy({},{});
  assertThrows(function(){ new proxy() }, TypeError);

  var proxy2 = new Proxy(proxy, {});
  assertThrows(function(){ proxy2() }, TypeError);
})();

(function testFailingConstructRevoked() {
  var pair = Proxy.revocable(Array, {});
  var instance = new pair.proxy();
  pair.revoke();
  assertThrows(function(){ new pair.proxy() }, TypeError);
})();

(function testFailingGetTrap() {
  var handler = {
    get() {
      throw TypeError();
    }
  }
  var proxy = new Proxy({},{});
  var proxy2 = new Proxy({}, proxy);
  assertThrows(function(){ new proxy2() }, TypeError);
})();

(function testConstructFallback() {
  var called = false;
  function Target() {
    called = true;
    this.property1 = 'value1';
  };
  Target.prototype = {};
  var proxy = new Proxy(Target, {});

  assertFalse(called);
  var instance = new proxy();
  assertTrue(called);
  assertEquals('value1', instance.property1);
  assertSame(Target.prototype, Reflect.getPrototypeOf(instance));

  var proxy2 = new Proxy(proxy, {});
  called = false;
  var instance2 = new proxy2();
  assertTrue(called);
  assertEquals('value1', instance2.property1);
  assertSame(Target.prototype, Reflect.getPrototypeOf(instance));
})();

(function testConstructTrapDirectReturn() {
  function Target(a, b) {
      this.sum = a + b;
  };
  var handler = {
      construct(t, c, args) {
          return { sum: 42 };
      }
  };
  var proxy = new Proxy(Target, handler);
  assertEquals(42, (new proxy(1, 2)).sum);
})();

(function testConstructTrap() {
  function Target(arg1, arg2) {
    this.arg1 = arg1;
    this.arg2 = arg2;
  }
  var seen_target, seen_arguments, seen_new_target;
  var handler = {
    construct(target, args, new_target) {
      seen_target = target;
      seen_arguments = args;
      seen_new_target = new_target;
      return Reflect.construct(target, args, new_target);
    }
  }
  var proxy = new Proxy(Target, handler);
  var instance = new proxy('a', 'b');
  assertEquals(Target, seen_target);
  assertEquals(['a','b'], seen_arguments);
  assertEquals(proxy, seen_new_target);
  assertEquals('a', instance.arg1);
  assertEquals('b', instance.arg2);

  var instance2 = Reflect.construct(proxy, ['a1', 'b1'], Array);
  assertEquals(Target, seen_target);
  assertEquals(['a1', 'b1'], seen_arguments);
  assertEquals(Array, seen_new_target);
  assertEquals('a1', instance2.arg1);
  assertEquals('b1', instance2.arg2);
})();

(function testConstructTrapNonConstructor() {
  function target() {
  };
  var p = new Proxy(target, {
    construct: function() {
      return 0;
    }
  });

  assertThrows(function() {
    new p();
  }, TypeError);
})();

(function testConstructCrossRealm() {
  var realm1 = Realm.create();
  var handler = {
    construct(target, args, new_target) {
      return args;
    }
  };
  var OtherProxy = Realm.eval(realm1, "Proxy");
  var otherArrayPrototype = Realm.eval(realm1, 'Array.prototype');

  // Proxy and handler are from this realm.
  var proxy = new Proxy(Array, handler);
  var result = new proxy();
  assertSame(Array.prototype, Reflect.getPrototypeOf(result));

  // Proxy is from this realm, handler is from realm1.
  var otherProxy = new OtherProxy(Array, handler);
  var otherResult = new otherProxy();
  assertSame(Array.prototype, Reflect.getPrototypeOf(otherResult));

  // Proxy and handler are from realm1.
  var otherProxy2 = Realm.eval(realm1, 'new Proxy('+
        'Array, { construct(target, args, new_target) { return args }} )');
  var otherResult2 = new otherProxy2();
  assertSame(Array.prototype, Reflect.getPrototypeOf(otherResult2));
})();

(function testReflectConstructCrossReal() {
  var realm1 = Realm.create();
  var realm2 = Realm.create();
  var realm3 = Realm.create();
  var realm4 = Realm.create();

  var argsRealm1 = Realm.eval(realm1, '[]');
  var ProxyRealm2 = Realm.eval(realm2, 'Proxy');
  var constructorRealm3 = Realm.eval(realm3, '(function(){})');
  var handlerRealm4 = Realm.eval(realm4,
      '({ construct(target, args, new_target) {return args} })');

  var proxy = new ProxyRealm2(constructorRealm3, handlerRealm4);

  // Check that the arguments array returned by handlerRealm4 is created in the
  // realm of the Reflect.construct function.
  var result = Reflect.construct(proxy, argsRealm1);
  assertSame(Array.prototype, Reflect.getPrototypeOf(result));

  var ReflectConstructRealm1 = Realm.eval(realm1, 'Reflect.construct');
  var result2 = ReflectConstructRealm1(proxy, argsRealm1);
  assertSame(Realm.eval(realm1, 'Array.prototype'),
    Reflect.getPrototypeOf(result2));

  var result3 = ReflectConstructRealm1(proxy, []);
  assertSame(Realm.eval(realm1, 'Array.prototype'),
    Reflect.getPrototypeOf(result3));

  var ReflectConstructRealm2 = Realm.eval(realm2, 'Reflect.construct');
  var result4 = ReflectConstructRealm2(proxy, argsRealm1);
  assertSame(Realm.eval(realm2, 'Array.prototype'),
    Reflect.getPrototypeOf(result4));
})();
