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

var target = {0: "a", 1: "b"};
var obj = new Proxy(target, handler);

log.length = 0;
assertEquals([obj], [].concat(obj));
assertEquals(1, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);

log.length = 0;
assertEquals([obj], Array.prototype.concat.apply(obj));
assertEquals(1, log.length);
for (var i in log) assertSame(target, log[i][1]);
assertEquals(["get", target, Symbol.isConcatSpreadable, obj], log[0]);
