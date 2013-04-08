// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --harmony-observation --harmony-proxies --harmony-collections
// Flags: --harmony-symbols --allow-natives-syntax

var allObservers = [];
function reset() {
  allObservers.forEach(function(observer) { observer.reset(); });
}

function stringifyNoThrow(arg) {
  try {
    return JSON.stringify(arg);
  } catch (e) {
    return '{<circular reference>}';
  }
}

function createObserver() {
  "use strict";  // So that |this| in callback can be undefined.

  var observer = {
    records: undefined,
    callbackCount: 0,
    reset: function() {
      this.records = undefined;
      this.callbackCount = 0;
    },
    assertNotCalled: function() {
      assertEquals(undefined, this.records);
      assertEquals(0, this.callbackCount);
    },
    assertCalled: function() {
      assertEquals(1, this.callbackCount);
    },
    assertRecordCount: function(count) {
      this.assertCalled();
      assertEquals(count, this.records.length);
    },
    assertCallbackRecords: function(recs) {
      this.assertRecordCount(recs.length);
      for (var i = 0; i < recs.length; i++) {
        if ('name' in recs[i]) recs[i].name = String(recs[i].name);
        print(i, stringifyNoThrow(this.records[i]), stringifyNoThrow(recs[i]));
        assertSame(this.records[i].object, recs[i].object);
        assertEquals('string', typeof recs[i].type);
        assertPropertiesEqual(this.records[i], recs[i]);
      }
    }
  };

  observer.callback = function(r) {
    assertEquals(undefined, this);
    assertEquals('object', typeof r);
    assertTrue(r instanceof Array)
    observer.records = r;
    observer.callbackCount++;
  };

  observer.reset();
  allObservers.push(observer);
  return observer;
}

var observer = createObserver();
assertEquals("function", typeof observer.callback);
var obj = {};

function frozenFunction() {}
Object.freeze(frozenFunction);
var nonFunction = {};
var changeRecordWithAccessor = { type: 'foo' };
var recordCreated = false;
Object.defineProperty(changeRecordWithAccessor, 'name', {
  get: function() {
    recordCreated = true;
    return "bar";
  },
  enumerable: true
})


// Object.observe
assertThrows(function() { Object.observe("non-object", observer.callback); }, TypeError);
assertThrows(function() { Object.observe(obj, nonFunction); }, TypeError);
assertThrows(function() { Object.observe(obj, frozenFunction); }, TypeError);
assertEquals(obj, Object.observe(obj, observer.callback));


// Object.unobserve
assertThrows(function() { Object.unobserve(4, observer.callback); }, TypeError);
assertThrows(function() { Object.unobserve(obj, nonFunction); }, TypeError);
assertEquals(obj, Object.unobserve(obj, observer.callback));


// Object.getNotifier
var notifier = Object.getNotifier(obj);
assertSame(notifier, Object.getNotifier(obj));
assertEquals(null, Object.getNotifier(Object.freeze({})));
assertFalse(notifier.hasOwnProperty('notify'));
assertEquals([], Object.keys(notifier));
var notifyDesc = Object.getOwnPropertyDescriptor(notifier.__proto__, 'notify');
assertTrue(notifyDesc.configurable);
assertTrue(notifyDesc.writable);
assertFalse(notifyDesc.enumerable);
assertThrows(function() { notifier.notify({}); }, TypeError);
assertThrows(function() { notifier.notify({ type: 4 }); }, TypeError);
var notify = notifier.notify;
assertThrows(function() { notify.call(undefined, { type: 'a' }); }, TypeError);
assertThrows(function() { notify.call(null, { type: 'a' }); }, TypeError);
assertThrows(function() { notify.call(5, { type: 'a' }); }, TypeError);
assertThrows(function() { notify.call('hello', { type: 'a' }); }, TypeError);
assertThrows(function() { notify.call(false, { type: 'a' }); }, TypeError);
assertThrows(function() { notify.call({}, { type: 'a' }); }, TypeError);
assertFalse(recordCreated);
notifier.notify(changeRecordWithAccessor);
assertFalse(recordCreated);  // not observed yet


// Object.deliverChangeRecords
assertThrows(function() { Object.deliverChangeRecords(nonFunction); }, TypeError);

Object.observe(obj, observer.callback);


// notify uses to [[CreateOwnProperty]] to create changeRecord;
reset();
var protoExpandoAccessed = false;
Object.defineProperty(Object.prototype, 'protoExpando',
  {
    configurable: true,
    set: function() { protoExpandoAccessed = true; }
  }
);
notifier.notify({ type: 'foo', protoExpando: 'val'});
assertFalse(protoExpandoAccessed);
delete Object.prototype.protoExpando;
Object.deliverChangeRecords(observer.callback);


// Multiple records are delivered.
reset();
notifier.notify({
  type: 'updated',
  name: 'foo',
  expando: 1
});

notifier.notify({
  object: notifier,  // object property is ignored
  type: 'deleted',
  name: 'bar',
  expando2: 'str'
});
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: 'foo', type: 'updated', expando: 1 },
  { object: obj, name: 'bar', type: 'deleted', expando2: 'str' }
]);


// No delivery takes place if no records are pending
reset();
Object.deliverChangeRecords(observer.callback);
observer.assertNotCalled();


// Multiple observation has no effect.
reset();
Object.observe(obj, observer.callback);
Object.observe(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
});
Object.deliverChangeRecords(observer.callback);
observer.assertCalled();


// Observation can be stopped.
reset();
Object.unobserve(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
});
Object.deliverChangeRecords(observer.callback);
observer.assertNotCalled();


// Multiple unobservation has no effect
reset();
Object.unobserve(obj, observer.callback);
Object.unobserve(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
});
Object.deliverChangeRecords(observer.callback);
observer.assertNotCalled();


// Re-observation works and only includes changeRecords after of call.
reset();
Object.getNotifier(obj).notify({
  type: 'foo',
});
Object.observe(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
});
records = undefined;
Object.deliverChangeRecords(observer.callback);
observer.assertRecordCount(1);


// Observing a continuous stream of changes, while itermittantly unobserving.
reset();
Object.observe(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
  val: 1
});

Object.unobserve(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
  val: 2
});

Object.observe(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
  val: 3
});

Object.unobserve(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
  val: 4
});

Object.observe(obj, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo',
  val: 5
});

Object.unobserve(obj, observer.callback);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, type: 'foo', val: 1 },
  { object: obj, type: 'foo', val: 3 },
  { object: obj, type: 'foo', val: 5 }
]);


// Observing multiple objects; records appear in order.
reset();
var obj2 = {};
var obj3 = {}
Object.observe(obj, observer.callback);
Object.observe(obj3, observer.callback);
Object.observe(obj2, observer.callback);
Object.getNotifier(obj).notify({
  type: 'foo1',
});
Object.getNotifier(obj2).notify({
  type: 'foo2',
});
Object.getNotifier(obj3).notify({
  type: 'foo3',
});
Object.observe(obj3, observer.callback);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, type: 'foo1' },
  { object: obj2, type: 'foo2' },
  { object: obj3, type: 'foo3' }
]);


// Recursive observation.
var obj = {a: 1};
var callbackCount = 0;
function recursiveObserver(r) {
  assertEquals(1, r.length);
  ++callbackCount;
  if (r[0].oldValue < 100) ++obj[r[0].name];
}
Object.observe(obj, recursiveObserver);
++obj.a;
Object.deliverChangeRecords(recursiveObserver);
assertEquals(100, callbackCount);

var obj1 = {a: 1};
var obj2 = {a: 1};
var recordCount = 0;
function recursiveObserver2(r) {
  recordCount += r.length;
  if (r[0].oldValue < 100) {
    ++obj1.a;
    ++obj2.a;
  }
}
Object.observe(obj1, recursiveObserver2);
Object.observe(obj2, recursiveObserver2);
++obj1.a;
Object.deliverChangeRecords(recursiveObserver2);
assertEquals(199, recordCount);


// Observing named properties.
reset();
var obj = {a: 1}
Object.observe(obj, observer.callback);
obj.a = 2;
obj["a"] = 3;
delete obj.a;
obj.a = 4;
obj.a = 4;  // ignored
obj.a = 5;
Object.defineProperty(obj, "a", {value: 6});
Object.defineProperty(obj, "a", {writable: false});
obj.a = 7;  // ignored
Object.defineProperty(obj, "a", {value: 8});
Object.defineProperty(obj, "a", {value: 7, writable: true});
Object.defineProperty(obj, "a", {get: function() {}});
Object.defineProperty(obj, "a", {get: frozenFunction});
Object.defineProperty(obj, "a", {get: frozenFunction});  // ignored
Object.defineProperty(obj, "a", {get: frozenFunction, set: frozenFunction});
Object.defineProperty(obj, "a", {set: frozenFunction});  // ignored
Object.defineProperty(obj, "a", {get: undefined, set: frozenFunction});
delete obj.a;
delete obj.a;
Object.defineProperty(obj, "a", {get: function() {}, configurable: true});
Object.defineProperty(obj, "a", {value: 9, writable: true});
obj.a = 10;
++obj.a;
obj.a++;
obj.a *= 3;
delete obj.a;
Object.defineProperty(obj, "a", {value: 11, configurable: true});
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: "a", type: "updated", oldValue: 1 },
  { object: obj, name: "a", type: "updated", oldValue: 2 },
  { object: obj, name: "a", type: "deleted", oldValue: 3 },
  { object: obj, name: "a", type: "new" },
  { object: obj, name: "a", type: "updated", oldValue: 4 },
  { object: obj, name: "a", type: "updated", oldValue: 5 },
  { object: obj, name: "a", type: "reconfigured" },
  { object: obj, name: "a", type: "updated", oldValue: 6 },
  { object: obj, name: "a", type: "reconfigured", oldValue: 8 },
  { object: obj, name: "a", type: "reconfigured", oldValue: 7 },
  { object: obj, name: "a", type: "reconfigured" },
  { object: obj, name: "a", type: "reconfigured" },
  { object: obj, name: "a", type: "reconfigured" },
  { object: obj, name: "a", type: "deleted" },
  { object: obj, name: "a", type: "new" },
  { object: obj, name: "a", type: "reconfigured" },
  { object: obj, name: "a", type: "updated", oldValue: 9 },
  { object: obj, name: "a", type: "updated", oldValue: 10 },
  { object: obj, name: "a", type: "updated", oldValue: 11 },
  { object: obj, name: "a", type: "updated", oldValue: 12 },
  { object: obj, name: "a", type: "deleted", oldValue: 36 },
  { object: obj, name: "a", type: "new" },
]);


// Observing indexed properties.
reset();
var obj = {'1': 1}
Object.observe(obj, observer.callback);
obj[1] = 2;
obj[1] = 3;
delete obj[1];
obj[1] = 4;
obj[1] = 4;  // ignored
obj[1] = 5;
Object.defineProperty(obj, "1", {value: 6});
Object.defineProperty(obj, "1", {writable: false});
obj[1] = 7;  // ignored
Object.defineProperty(obj, "1", {value: 8});
Object.defineProperty(obj, "1", {value: 7, writable: true});
Object.defineProperty(obj, "1", {get: function() {}});
Object.defineProperty(obj, "1", {get: frozenFunction});
Object.defineProperty(obj, "1", {get: frozenFunction});  // ignored
Object.defineProperty(obj, "1", {get: frozenFunction, set: frozenFunction});
Object.defineProperty(obj, "1", {set: frozenFunction});  // ignored
Object.defineProperty(obj, "1", {get: undefined, set: frozenFunction});
delete obj[1];
delete obj[1];
Object.defineProperty(obj, "1", {get: function() {}, configurable: true});
Object.defineProperty(obj, "1", {value: 9, writable: true});
obj[1] = 10;
++obj[1];
obj[1]++;
obj[1] *= 3;
delete obj[1];
Object.defineProperty(obj, "1", {value: 11, configurable: true});
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: "1", type: "updated", oldValue: 1 },
  { object: obj, name: "1", type: "updated", oldValue: 2 },
  { object: obj, name: "1", type: "deleted", oldValue: 3 },
  { object: obj, name: "1", type: "new" },
  { object: obj, name: "1", type: "updated", oldValue: 4 },
  { object: obj, name: "1", type: "updated", oldValue: 5 },
  { object: obj, name: "1", type: "reconfigured" },
  { object: obj, name: "1", type: "updated", oldValue: 6 },
  { object: obj, name: "1", type: "reconfigured", oldValue: 8 },
  { object: obj, name: "1", type: "reconfigured", oldValue: 7 },
  { object: obj, name: "1", type: "reconfigured" },
  { object: obj, name: "1", type: "reconfigured" },
  { object: obj, name: "1", type: "reconfigured" },
  { object: obj, name: "1", type: "deleted" },
  { object: obj, name: "1", type: "new" },
  { object: obj, name: "1", type: "reconfigured" },
  { object: obj, name: "1", type: "updated", oldValue: 9 },
  { object: obj, name: "1", type: "updated", oldValue: 10 },
  { object: obj, name: "1", type: "updated", oldValue: 11 },
  { object: obj, name: "1", type: "updated", oldValue: 12 },
  { object: obj, name: "1", type: "deleted", oldValue: 36 },
  { object: obj, name: "1", type: "new" },
]);


// Observing symbol properties (not).
print("*****")
reset();
var obj = {}
var symbol = Symbol("secret");
Object.observe(obj, observer.callback);
obj[symbol] = 3;
delete obj[symbol];
Object.defineProperty(obj, symbol, {get: function() {}, configurable: true});
Object.defineProperty(obj, symbol, {value: 6});
Object.defineProperty(obj, symbol, {writable: false});
delete obj[symbol];
Object.defineProperty(obj, symbol, {value: 7});
++obj[symbol];
obj[symbol]++;
obj[symbol] *= 3;
delete obj[symbol];
obj.__defineSetter__(symbol, function() {});
obj.__defineGetter__(symbol, function() {});
Object.deliverChangeRecords(observer.callback);
observer.assertNotCalled();


// Test all kinds of objects generically.
function TestObserveConfigurable(obj, prop) {
  reset();
  obj[prop] = 1;
  Object.observe(obj, observer.callback);
  obj[prop] = 2;
  obj[prop] = 3;
  delete obj[prop];
  obj[prop] = 4;
  obj[prop] = 4;  // ignored
  obj[prop] = 5;
  Object.defineProperty(obj, prop, {value: 6});
  Object.defineProperty(obj, prop, {writable: false});
  obj[prop] = 7;  // ignored
  Object.defineProperty(obj, prop, {value: 8});
  Object.defineProperty(obj, prop, {value: 7, writable: true});
  Object.defineProperty(obj, prop, {get: function() {}});
  Object.defineProperty(obj, prop, {get: frozenFunction});
  Object.defineProperty(obj, prop, {get: frozenFunction});  // ignored
  Object.defineProperty(obj, prop, {get: frozenFunction, set: frozenFunction});
  Object.defineProperty(obj, prop, {set: frozenFunction});  // ignored
  Object.defineProperty(obj, prop, {get: undefined, set: frozenFunction});
  obj.__defineSetter__(prop, frozenFunction);  // ignored
  obj.__defineSetter__(prop, function() {});
  obj.__defineGetter__(prop, function() {});
  delete obj[prop];
  delete obj[prop];  // ignored
  obj.__defineGetter__(prop, function() {});
  delete obj[prop];
  Object.defineProperty(obj, prop, {get: function() {}, configurable: true});
  Object.defineProperty(obj, prop, {value: 9, writable: true});
  obj[prop] = 10;
  ++obj[prop];
  obj[prop]++;
  obj[prop] *= 3;
  delete obj[prop];
  Object.defineProperty(obj, prop, {value: 11, configurable: true});
  Object.deliverChangeRecords(observer.callback);
  observer.assertCallbackRecords([
    { object: obj, name: prop, type: "updated", oldValue: 1 },
    { object: obj, name: prop, type: "updated", oldValue: 2 },
    { object: obj, name: prop, type: "deleted", oldValue: 3 },
    { object: obj, name: prop, type: "new" },
    { object: obj, name: prop, type: "updated", oldValue: 4 },
    { object: obj, name: prop, type: "updated", oldValue: 5 },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "updated", oldValue: 6 },
    { object: obj, name: prop, type: "reconfigured", oldValue: 8 },
    { object: obj, name: prop, type: "reconfigured", oldValue: 7 },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "deleted" },
    { object: obj, name: prop, type: "new" },
    { object: obj, name: prop, type: "deleted" },
    { object: obj, name: prop, type: "new" },
    { object: obj, name: prop, type: "reconfigured" },
    { object: obj, name: prop, type: "updated", oldValue: 9 },
    { object: obj, name: prop, type: "updated", oldValue: 10 },
    { object: obj, name: prop, type: "updated", oldValue: 11 },
    { object: obj, name: prop, type: "updated", oldValue: 12 },
    { object: obj, name: prop, type: "deleted", oldValue: 36 },
    { object: obj, name: prop, type: "new" },
  ]);
  Object.unobserve(obj, observer.callback);
  delete obj[prop];
}

function TestObserveNonConfigurable(obj, prop, desc) {
  reset();
  obj[prop] = 1;
  Object.observe(obj, observer.callback);
  obj[prop] = 4;
  obj[prop] = 4;  // ignored
  obj[prop] = 5;
  Object.defineProperty(obj, prop, {value: 6});
  Object.defineProperty(obj, prop, {value: 6});  // ignored
  Object.defineProperty(obj, prop, {value: 7});
  Object.defineProperty(obj, prop, {enumerable: desc.enumerable});  // ignored
  Object.defineProperty(obj, prop, {writable: false});
  obj[prop] = 7;  // ignored
  Object.deliverChangeRecords(observer.callback);
  observer.assertCallbackRecords([
    { object: obj, name: prop, type: "updated", oldValue: 1 },
    { object: obj, name: prop, type: "updated", oldValue: 4 },
    { object: obj, name: prop, type: "updated", oldValue: 5 },
    { object: obj, name: prop, type: "updated", oldValue: 6 },
    { object: obj, name: prop, type: "reconfigured" },
  ]);
  Object.unobserve(obj, observer.callback);
}

function createProxy(create, x) {
  var handler = {
    getPropertyDescriptor: function(k) {
      for (var o = this.target; o; o = Object.getPrototypeOf(o)) {
        var desc = Object.getOwnPropertyDescriptor(o, k);
        if (desc) return desc;
      }
      return undefined;
    },
    getOwnPropertyDescriptor: function(k) {
      return Object.getOwnPropertyDescriptor(this.target, k);
    },
    defineProperty: function(k, desc) {
      var x = Object.defineProperty(this.target, k, desc);
      Object.deliverChangeRecords(this.callback);
      return x;
    },
    delete: function(k) {
      var x = delete this.target[k];
      Object.deliverChangeRecords(this.callback);
      return x;
    },
    getPropertyNames: function() {
      return Object.getOwnPropertyNames(this.target);
    },
    target: {isProxy: true},
    callback: function(changeRecords) {
      print("callback", stringifyNoThrow(handler.proxy), stringifyNoThrow(got));
      for (var i in changeRecords) {
        var got = changeRecords[i];
        var change = {object: handler.proxy, name: got.name, type: got.type};
        if ("oldValue" in got) change.oldValue = got.oldValue;
        Object.getNotifier(handler.proxy).notify(change);
      }
    },
  };
  Object.observe(handler.target, handler.callback);
  return handler.proxy = create(handler, x);
}

var objects = [
  {},
  [],
  this,  // global object
  function(){},
  (function(){ return arguments })(),
  (function(){ "use strict"; return arguments })(),
  Object(1), Object(true), Object("bla"),
  new Date(),
  Object, Function, Date, RegExp,
  new Set, new Map, new WeakMap,
  new ArrayBuffer(10), new Int32Array(5),
  createProxy(Proxy.create, null),
  createProxy(Proxy.createFunction, function(){}),
];
var properties = ["a", "1", 1, "length", "prototype", "name", "caller"];

// Cases that yield non-standard results.
function blacklisted(obj, prop) {
  return (obj instanceof Int32Array && prop == 1) ||
         (obj instanceof Int32Array && prop === "length") ||
         (obj instanceof ArrayBuffer && prop == 1)
}

for (var i in objects) for (var j in properties) {
  var obj = objects[i];
  var prop = properties[j];
  if (blacklisted(obj, prop)) continue;
  var desc = Object.getOwnPropertyDescriptor(obj, prop);
  print("***", typeof obj, stringifyNoThrow(obj), prop);
  if (!desc || desc.configurable)
    TestObserveConfigurable(obj, prop);
  else if (desc.writable)
    TestObserveNonConfigurable(obj, prop, desc);
}


// Observing array length (including truncation)
reset();
var arr = ['a', 'b', 'c', 'd'];
var arr2 = ['alpha', 'beta'];
var arr3 = ['hello'];
arr3[2] = 'goodbye';
arr3.length = 6;
var slow_arr = new Array(1000000000);
slow_arr[500000000] = 'hello';
Object.defineProperty(arr, '0', {configurable: false});
Object.defineProperty(arr, '2', {get: function(){}});
Object.defineProperty(arr2, '0', {get: function(){}, configurable: false});
Object.observe(arr, observer.callback);
Object.observe(arr2, observer.callback);
Object.observe(arr3, observer.callback);
Object.observe(slow_arr, observer.callback);
arr.length = 2;
arr.length = 0;
arr.length = 10;
Object.defineProperty(arr, 'length', {writable: false});
arr2.length = 0;
arr2.length = 1; // no change expected
Object.defineProperty(arr2, 'length', {value: 1, writable: false});
arr3.length = 0;
++arr3.length;
arr3.length++;
arr3.length /= 2;
Object.defineProperty(arr3, 'length', {value: 5});
Object.defineProperty(arr3, 'length', {value: 10, writable: false});
slow_arr.length = 100;
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: arr, name: '3', type: 'deleted', oldValue: 'd' },
  { object: arr, name: '2', type: 'deleted' },
  { object: arr, name: 'length', type: 'updated', oldValue: 4 },
  { object: arr, name: '1', type: 'deleted', oldValue: 'b' },
  { object: arr, name: 'length', type: 'updated', oldValue: 2 },
  { object: arr, name: 'length', type: 'updated', oldValue: 1 },
  { object: arr, name: 'length', type: 'reconfigured' },
  { object: arr2, name: '1', type: 'deleted', oldValue: 'beta' },
  { object: arr2, name: 'length', type: 'updated', oldValue: 2 },
  { object: arr2, name: 'length', type: 'reconfigured', oldValue: 1 },
  { object: arr3, name: '2', type: 'deleted', oldValue: 'goodbye' },
  { object: arr3, name: '0', type: 'deleted', oldValue: 'hello' },
  { object: arr3, name: 'length', type: 'updated', oldValue: 6 },
  { object: arr3, name: 'length', type: 'updated', oldValue: 0 },
  { object: arr3, name: 'length', type: 'updated', oldValue: 1 },
  { object: arr3, name: 'length', type: 'updated', oldValue: 2 },
  { object: arr3, name: 'length', type: 'updated', oldValue: 1 },
  { object: arr3, name: 'length', type: 'reconfigured', oldValue: 5 },
  { object: slow_arr, name: '500000000', type: 'deleted', oldValue: 'hello' },
  { object: slow_arr, name: 'length', type: 'updated', oldValue: 1000000000 },
]);


// Assignments in loops (checking different IC states).
reset();
var obj = {};
Object.observe(obj, observer.callback);
for (var i = 0; i < 5; i++) {
  obj["a" + i] = i;
}
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: "a0", type: "new" },
  { object: obj, name: "a1", type: "new" },
  { object: obj, name: "a2", type: "new" },
  { object: obj, name: "a3", type: "new" },
  { object: obj, name: "a4", type: "new" },
]);

reset();
var obj = {};
Object.observe(obj, observer.callback);
for (var i = 0; i < 5; i++) {
  obj[i] = i;
}
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: "0", type: "new" },
  { object: obj, name: "1", type: "new" },
  { object: obj, name: "2", type: "new" },
  { object: obj, name: "3", type: "new" },
  { object: obj, name: "4", type: "new" },
]);


// Adding elements past the end of an array should notify on length
reset();
var arr = [1, 2, 3];
Object.observe(arr, observer.callback);
arr[3] = 10;
arr[100] = 20;
Object.defineProperty(arr, '200', {value: 7});
Object.defineProperty(arr, '400', {get: function(){}});
arr[50] = 30; // no length change expected
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: arr, name: '3', type: 'new' },
  { object: arr, name: 'length', type: 'updated', oldValue: 3 },
  { object: arr, name: '100', type: 'new' },
  { object: arr, name: 'length', type: 'updated', oldValue: 4 },
  { object: arr, name: '200', type: 'new' },
  { object: arr, name: 'length', type: 'updated', oldValue: 101 },
  { object: arr, name: '400', type: 'new' },
  { object: arr, name: 'length', type: 'updated', oldValue: 201 },
  { object: arr, name: '50', type: 'new' },
]);


// Tests for array methods, first on arrays and then on plain objects
//
// === ARRAYS ===
//
// Push
reset();
var array = [1, 2];
Object.observe(array, observer.callback);
array.push(3, 4);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '2', type: 'new' },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '3', type: 'new' },
  { object: array, name: 'length', type: 'updated', oldValue: 3 },
]);

// Pop
reset();
var array = [1, 2];
Object.observe(array, observer.callback);
array.pop();
array.pop();
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '1', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '0', type: 'deleted', oldValue: 1 },
  { object: array, name: 'length', type: 'updated', oldValue: 1 },
]);

// Shift
reset();
var array = [1, 2];
Object.observe(array, observer.callback);
array.shift();
array.shift();
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '0', type: 'updated', oldValue: 1 },
  { object: array, name: '1', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '0', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 1 },
]);

// Unshift
reset();
var array = [1, 2];
Object.observe(array, observer.callback);
array.unshift(3, 4);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '3', type: 'new' },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '2', type: 'new' },
  { object: array, name: '0', type: 'updated', oldValue: 1 },
  { object: array, name: '1', type: 'updated', oldValue: 2 },
]);

// Splice
reset();
var array = [1, 2, 3];
Object.observe(array, observer.callback);
array.splice(1, 1, 4, 5);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '3', type: 'new' },
  { object: array, name: 'length', type: 'updated', oldValue: 3 },
  { object: array, name: '1', type: 'updated', oldValue: 2 },
  { object: array, name: '2', type: 'updated', oldValue: 3 },
]);


//
// === PLAIN OBJECTS ===
//
// Push
reset()
var array = {0: 1, 1: 2, length: 2}
Object.observe(array, observer.callback);
Array.prototype.push.call(array, 3, 4);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '2', type: 'new' },
  { object: array, name: '3', type: 'new' },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
]);

// Pop
reset()
var array = {0: 1, 1: 2, length: 2};
Object.observe(array, observer.callback);
Array.prototype.pop.call(array);
Array.prototype.pop.call(array);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '1', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '0', type: 'deleted', oldValue: 1 },
  { object: array, name: 'length', type: 'updated', oldValue: 1 },
]);

// Shift
reset()
var array = {0: 1, 1: 2, length: 2};
Object.observe(array, observer.callback);
Array.prototype.shift.call(array);
Array.prototype.shift.call(array);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '0', type: 'updated', oldValue: 1 },
  { object: array, name: '1', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
  { object: array, name: '0', type: 'deleted', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 1 },
]);

// Unshift
reset()
var array = {0: 1, 1: 2, length: 2};
Object.observe(array, observer.callback);
Array.prototype.unshift.call(array, 3, 4);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '3', type: 'new' },
  { object: array, name: '2', type: 'new' },
  { object: array, name: '0', type: 'updated', oldValue: 1 },
  { object: array, name: '1', type: 'updated', oldValue: 2 },
  { object: array, name: 'length', type: 'updated', oldValue: 2 },
]);

// Splice
reset()
var array = {0: 1, 1: 2, 2: 3, length: 3};
Object.observe(array, observer.callback);
Array.prototype.splice.call(array, 1, 1, 4, 5);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '3', type: 'new' },
  { object: array, name: '1', type: 'updated', oldValue: 2 },
  { object: array, name: '2', type: 'updated', oldValue: 3 },
  { object: array, name: 'length', type: 'updated', oldValue: 3 },
]);

// Exercise StoreIC_ArrayLength
reset();
var dummy = {};
Object.observe(dummy, observer.callback);
Object.unobserve(dummy, observer.callback);
var array = [0];
Object.observe(array, observer.callback);
array.splice(0, 1);
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: array, name: '0', type: 'deleted', oldValue: 0 },
  { object: array, name: 'length', type: 'updated', oldValue: 1},
]);


// __proto__
reset();
var obj = {};
Object.observe(obj, observer.callback);
var p = {foo: 'yes'};
var q = {bar: 'no'};
obj.__proto__ = p;
obj.__proto__ = p;  // ignored
obj.__proto__ = null;
obj.__proto__ = q;  // the __proto__ accessor is gone
// TODO(adamk): Add tests for objects with hidden prototypes
// once we support observing the global object.
Object.deliverChangeRecords(observer.callback);
observer.assertCallbackRecords([
  { object: obj, name: '__proto__', type: 'prototype',
    oldValue: Object.prototype },
  { object: obj, name: '__proto__', type: 'prototype', oldValue: p },
  { object: obj, name: '__proto__', type: 'new' },
]);


// Function.prototype
reset();
var fun = function(){};
Object.observe(fun, observer.callback);
var myproto = {foo: 'bar'};
fun.prototype = myproto;
fun.prototype = 7;
fun.prototype = 7;  // ignored
Object.defineProperty(fun, 'prototype', {value: 8});
Object.deliverChangeRecords(observer.callback);
observer.assertRecordCount(3);
// Manually examine the first record in order to test
// lazy creation of oldValue
assertSame(fun, observer.records[0].object);
assertEquals('prototype', observer.records[0].name);
assertEquals('updated', observer.records[0].type);
// The only existing reference to the oldValue object is in this
// record, so to test that lazy creation happened correctly
// we compare its constructor to our function (one of the invariants
// ensured when creating an object via AllocateFunctionPrototype).
assertSame(fun, observer.records[0].oldValue.constructor);
observer.records.splice(0, 1);
observer.assertCallbackRecords([
  { object: fun, name: 'prototype', type: 'updated', oldValue: myproto },
  { object: fun, name: 'prototype', type: 'updated', oldValue: 7 },
]);

// Function.prototype should not be observable except on the object itself
reset();
var fun = function(){};
var obj = { __proto__: fun };
Object.observe(obj, observer.callback);
obj.prototype = 7;
Object.deliverChangeRecords(observer.callback);
observer.assertNotCalled();


// Check that changes in observation status are detected in all IC states and
// in optimized code, especially in cases usually using fast elements.
var mutation = [
  "a[i] = v",
  "a[i] ? ++a[i] : a[i] = v",
  "a[i] ? a[i]++ : a[i] = v",
  "a[i] ? a[i] += 1 : a[i] = v",
  "a[i] ? a[i] -= -1 : a[i] = v",
];

var props = [1, "1", "a"];

function TestFastElements(prop, mutation, prepopulate, polymorphic, optimize) {
  var setElement = eval(
    "(function setElement(a, i, v) { " + mutation + "; " +
    "/* " + [].join.call(arguments, " ") + " */" +
    "})"
  );
  print("TestFastElements:", setElement);

  var arr = prepopulate ? [1, 2, 3, 4, 5] : [0];
  if (prepopulate) arr[prop] = 2;  // for non-element case
  setElement(arr, prop, 3);
  setElement(arr, prop, 4);
  if (polymorphic) setElement(["M", "i", "l", "n", "e", "r"], 0, "m");
  if (optimize) %OptimizeFunctionOnNextCall(setElement);
  setElement(arr, prop, 5);

  reset();
  Object.observe(arr, observer.callback);
  setElement(arr, prop, 989898);
  Object.deliverChangeRecords(observer.callback);
  observer.assertCallbackRecords([
    { object: arr, name: "" + prop, type: 'updated', oldValue: 5 }
  ]);
}

for (var b1 = 0; b1 < 2; ++b1)
  for (var b2 = 0; b2 < 2; ++b2)
    for (var b3 = 0; b3 < 2; ++b3)
      for (var i in props)
        for (var j in mutation)
          TestFastElements(props[i], mutation[j], b1 != 0, b2 != 0, b3 != 0);


var mutation = [
  "a.length = v",
  "a.length += newSize - oldSize",
  "a.length -= oldSize - newSize",
];

var mutationByIncr = [
  "++a.length",
  "a.length++",
];

function TestFastElementsLength(
  mutation, polymorphic, optimize, oldSize, newSize) {
  var setLength = eval(
    "(function setLength(a, v) { " + mutation + "; " +
    "/* " + [].join.call(arguments, " ") + " */"
    + "})"
  );
  print("TestFastElementsLength:", setLength);

  function array(n) {
    var arr = new Array(n);
    for (var i = 0; i < n; ++i) arr[i] = i;
    return arr;
  }

  setLength(array(oldSize), newSize);
  setLength(array(oldSize), newSize);
  if (polymorphic) setLength(array(oldSize).map(isNaN), newSize);
  if (optimize) %OptimizeFunctionOnNextCall(setLength);
  setLength(array(oldSize), newSize);

  reset();
  var arr = array(oldSize);
  Object.observe(arr, observer.callback);
  setLength(arr, newSize);
  Object.deliverChangeRecords(observer.callback);
  if (oldSize === newSize) {
    observer.assertNotCalled();
  } else {
    var count = oldSize > newSize ? oldSize - newSize : 0;
    observer.assertRecordCount(count + 1);
    var lengthRecord = observer.records[count];
    assertSame(arr, lengthRecord.object);
    assertEquals('length', lengthRecord.name);
    assertEquals('updated', lengthRecord.type);
    assertSame(oldSize, lengthRecord.oldValue);
  }
}

for (var b1 = 0; b1 < 2; ++b1)
  for (var b2 = 0; b2 < 2; ++b2)
    for (var n1 = 0; n1 < 3; ++n1)
      for (var n2 = 0; n2 < 3; ++n2)
        for (var i in mutation)
          TestFastElementsLength(mutation[i], b1 != 0, b2 != 0, 20*n1, 20*n2);

for (var b1 = 0; b1 < 2; ++b1)
  for (var b2 = 0; b2 < 2; ++b2)
    for (var n = 0; n < 3; ++n)
      for (var i in mutationByIncr)
        TestFastElementsLength(mutationByIncr[i], b1 != 0, b2 != 0, 7*n, 7*n+1);
