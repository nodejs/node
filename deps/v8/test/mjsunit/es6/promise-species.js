// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that Promises use @@species appropriately

// Another constructor with no species will not be instantiated
var test = new Promise(function(){});
var bogoCount = 0;
function bogusConstructor() { bogoCount++; }
test.constructor = bogusConstructor;
assertTrue(Promise.resolve(test) instanceof Promise);
assertFalse(Promise.resolve(test) instanceof bogusConstructor);
// Tests that chromium:575314 is fixed thoroughly
Promise.resolve(test).catch(e => %AbortJS("Error " + e)).then(() => {
  if (bogoCount != 0) %AbortJS("bogoCount was " + bogoCount + " should be 0");
});

// If there is a species, it will be instantiated
// @@species will be read exactly once, and the constructor is called with a
// function
var count = 0;
var params;
class MyPromise extends Promise {
  constructor(...args) {
    super(...args);
    params = args;
  }
  static get [Symbol.species]() {
    count++
    return this;
  }
}

var myPromise = MyPromise.resolve().then();
assertEquals(1, count);
assertEquals(1, params.length);
assertEquals('function', typeof(params[0]));
assertTrue(myPromise instanceof MyPromise);
assertTrue(myPromise instanceof Promise);
