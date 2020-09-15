// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A non-callable reject function throws eagerly
var log = [];
var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

function MyPromise(resolver) {
  var reject = undefined;
  var resolve = function() { };
  resolver(resolve, reject);
};

MyPromise.prototype = new Promise(function() {});
MyPromise.__proto__ = Promise;
p.constructor = MyPromise;

assertThrows(()=> p.then(function() { }), TypeError);
