// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: concat does not currently support species so there is no difference
// between [].concat(foo) and Array.prototype.concat.apply(foo).

var log = [];
var logger = {};
var handler = new Proxy({}, logger);

logger.get = function(t, trap, r) {
  return function(...args) {
    log.push([trap, ...args]);
    return Reflect[trap](...args);
  }
};

var target = {0: "a", 1: "b", [Symbol.isConcatSpreadable]: "truish"};
var obj = new Proxy(target, handler);

log.length = 0;
assertEquals([], [].concat(obj));
assertEquals(2, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);
assertEquals(["get", target, "length", obj], log[1]);

log.length = 0;
assertEquals([], Array.prototype.concat.apply(obj));
assertEquals(2, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);
assertEquals(["get", target, "length", obj], log[1]);

target.length = 3;

log.length = 0;
assertEquals(["a", "b", undefined], [].concat(obj));
assertEquals(7, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);
assertEquals(["get", target, "length", obj], log[1]);
assertEquals(["has", target, "0"], log[2]);
assertEquals(["get", target, "0", obj], log[3]);
assertEquals(["has", target, "1"], log[4]);
assertEquals(["get", target, "1", obj], log[5]);
assertEquals(["has", target, "2"], log[6]);

log.length = 0;
assertEquals(["a", "b", undefined], Array.prototype.concat.apply(obj));
assertEquals(7, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);
assertEquals(["get", target, "length", obj], log[1]);
assertEquals(["has", target, "0"], log[2]);
assertEquals(["get", target, "0", obj], log[3]);
assertEquals(["has", target, "1"], log[4]);
assertEquals(["get", target, "1", obj], log[5]);
assertEquals(["has", target, "2"], log[6]);
