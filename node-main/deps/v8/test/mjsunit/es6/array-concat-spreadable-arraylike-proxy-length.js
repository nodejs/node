// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: concat does not currently support species so there is no difference
// between [].concat(foo) and Array.prototype.concat.apply(foo).

var getTrap = function(t, key) {
  if (key === "length") return {[Symbol.toPrimitive]() {return 3}};
  if (key === "2") return "baz";
  if (key === "3") return "bar";
};
var target = [];
var obj = new Proxy(target, {get: getTrap, has: () => true});

assertEquals([undefined, undefined, "baz"], [].concat(obj));
assertEquals([undefined, undefined, "baz"], Array.prototype.concat.apply(obj))
