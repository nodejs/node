// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the ES2015 @@species feature

'use strict';

// Subclasses of Array construct themselves under map, etc

class MyArray extends Array { }

assertEquals(MyArray, new MyArray().map(()=>{}).constructor);
assertEquals(MyArray, new MyArray().filter(()=>{}).constructor);
assertEquals(MyArray, new MyArray().slice().constructor);
assertEquals(MyArray, new MyArray().splice().constructor);
assertEquals(MyArray, new MyArray().concat([1]).constructor);
assertEquals(1, new MyArray().concat([1])[0]);

// Subclasses can override @@species to return the another class

class MyOtherArray extends Array {
  static get [Symbol.species]() { return MyArray; }
}

assertEquals(MyArray, new MyOtherArray().map(()=>{}).constructor);
assertEquals(MyArray, new MyOtherArray().filter(()=>{}).constructor);
assertEquals(MyArray, new MyOtherArray().slice().constructor);
assertEquals(MyArray, new MyOtherArray().splice().constructor);
assertEquals(MyArray, new MyOtherArray().concat().constructor);

// Array  methods on non-arrays return arrays

class MyNonArray extends Array {
  static get [Symbol.species]() { return MyObject; }
}

class MyObject { }

assertEquals(MyObject,
             Array.prototype.map.call(new MyNonArray(), ()=>{}).constructor);
assertEquals(MyObject,
             Array.prototype.filter.call(new MyNonArray(), ()=>{}).constructor);
assertEquals(MyObject,
             Array.prototype.slice.call(new MyNonArray()).constructor);
assertEquals(MyObject,
             Array.prototype.splice.call(new MyNonArray()).constructor);
assertEquals(MyObject,
             Array.prototype.concat.call(new MyNonArray()).constructor);

assertEquals(undefined,
             Array.prototype.map.call(new MyNonArray(), ()=>{}).length);
assertEquals(undefined,
             Array.prototype.filter.call(new MyNonArray(), ()=>{}).length);
// slice, splice, and concat actually do explicitly define the length.
assertEquals(0, Array.prototype.slice.call(new MyNonArray()).length);
assertEquals(0, Array.prototype.splice.call(new MyNonArray()).length);
assertEquals(1, Array.prototype.concat.call(new MyNonArray(), ()=>{}).length);

// Cross-realm Arrays build same-realm arrays

var realm = Realm.create();
assertEquals(Array,
             Array.prototype.map.call(
                 Realm.eval(realm, "[]"), ()=>{}).constructor);
assertFalse(Array === Realm.eval(realm, "[]").map(()=>{}).constructor);
assertFalse(Array === Realm.eval(realm, "[].map(()=>{}).constructor"));
assertEquals(Array,
             Array.prototype.concat.call(
                 Realm.eval(realm, "[]")).constructor);

// Defaults when constructor or @@species is missing or non-constructor

class MyDefaultArray extends Array {
  static get [Symbol.species]() { return undefined; }
}
assertEquals(Array, new MyDefaultArray().map(()=>{}).constructor);

class MyOtherDefaultArray extends Array { }
assertEquals(MyOtherDefaultArray,
             new MyOtherDefaultArray().map(()=>{}).constructor);
MyOtherDefaultArray.prototype.constructor = undefined;
assertEquals(Array, new MyOtherDefaultArray().map(()=>{}).constructor);
assertEquals(Array, new MyOtherDefaultArray().concat().constructor);

// Exceptions propagated when getting constructor @@species throws

class SpeciesError extends Error { }
class ConstructorError extends Error { }
class MyThrowingArray extends Array {
  static get [Symbol.species]() { throw new SpeciesError; }
}
assertThrows(() => new MyThrowingArray().map(()=>{}), SpeciesError);
Object.defineProperty(MyThrowingArray.prototype, 'constructor', {
    get() { throw new ConstructorError; }
});
assertThrows(() => new MyThrowingArray().map(()=>{}), ConstructorError);

// Previously unexpected errors from setting properties in arrays throw

class FrozenArray extends Array {
  constructor(...args) {
    super(...args);
    Object.freeze(this);
  }
}
assertThrows(() => new FrozenArray([1]).map(()=>0), TypeError);
assertThrows(() => new FrozenArray([1]).filter(()=>true), TypeError);
assertThrows(() => new FrozenArray([1]).slice(0, 1), TypeError);
assertThrows(() => new FrozenArray([1]).splice(0, 1), TypeError);
assertThrows(() => new FrozenArray([]).concat([1]), TypeError);

// Verify call counts and constructor parameters

var count;
var params;
class MyObservedArray extends Array {
  constructor(...args) {
    super(...args);
    params = args;
  }
  static get [Symbol.species]() {
    count++
    return this;
  }
}

count = 0;
params = undefined;
assertEquals(MyObservedArray,
             new MyObservedArray().map(()=>{}).constructor);
assertEquals(1, count);
assertArrayEquals([0], params);

count = 0;
params = undefined;
assertEquals(MyObservedArray,
             new MyObservedArray().filter(()=>{}).constructor);
assertEquals(1, count);
assertArrayEquals([0], params);

count = 0;
params = undefined;
assertEquals(MyObservedArray,
             new MyObservedArray().concat().constructor);
assertEquals(1, count);
assertArrayEquals([0], params);

count = 0;
params = undefined;
assertEquals(MyObservedArray,
             new MyObservedArray().slice().constructor);
assertEquals(1, count);
assertArrayEquals([0], params);

count = 0;
params = undefined;
assertEquals(MyObservedArray,
             new MyObservedArray().splice().constructor);
assertEquals(1, count);
assertArrayEquals([0], params);

// @@species constructor can be a Proxy, and the realm access doesn't
// crash

class MyProxyArray extends Array { }
let ProxyArray = new Proxy(MyProxyArray, {});
MyProxyArray.constructor = ProxyArray;

assertEquals(MyProxyArray, new ProxyArray().map(()=>{}).constructor);
