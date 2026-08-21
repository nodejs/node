// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function should_not_be_called() {
  throw new Error("should not be called");
}

(function() {
  var r = new RegExp("biep");
  assertTrue(r === RegExp(r));
  assertFalse(r === new RegExp(r));
  r[Symbol.match] = false;
  Object.defineProperty(r, "source", {get: should_not_be_called});
  Object.defineProperty(r, "flags", {get: should_not_be_called});
  assertFalse(r === RegExp(r));
})();

(function() {
  let allow = false;
  class A extends RegExp {
    get source() {
      if (!allow) throw new Error("should not be called");
      return super.source;
    }
    get flags() {
      if (!allow) throw new Error("should not be called");
      return super.flags
    }
  }

  var r = new A("biep");
  var r2 = RegExp(r);

  assertFalse(r === r2);
  allow = true;
  assertEquals(r, r2);
  allow = false;
  assertTrue(A.prototype === r.__proto__);
  assertTrue(RegExp.prototype === r2.__proto__);

  var r3 = RegExp(r);
  assertFalse(r3 === r);
  allow = true;
  assertEquals(r3, r);
  allow = false;

  var r4 = new A(r2);
  assertFalse(r4 === r2);
  allow = true;
  assertEquals(r4, r2);
  allow = false;
  assertTrue(A.prototype === r4.__proto__);

  r[Symbol.match] = false;
  var r5 = new A(r);
  assertFalse(r5 === r);
  allow = true;
  assertEquals(r5, r);
  allow = false;
  assertTrue(A.prototype === r5.__proto__);
})();

(function() {
  var log = [];
  var match = {
    get source() { log.push("source"); return "biep"; },
    get flags() { log.push("flags"); return "i"; }
  };
  Object.defineProperty(match, Symbol.match,
      {get() { log.push("match"); return true; }});
  var r = RegExp(match);
  assertEquals(["match", "source", "flags"], log);
  assertFalse(r === match);
  assertEquals(/biep/i, r);
})();

(function() {
  var log = [];
  var match = {
    get source() { log.push("source"); return "biep"; },
    get flags() { log.push("flags"); return "i"; }
  };
  Object.defineProperty(match, Symbol.match,
      {get() { log.push("match"); return true; }});
  match.constructor = RegExp;
  var r = RegExp(match);
  assertEquals(["match"], log);
  assertTrue(r === match);
})();

(function() {
  var r = RegExp("biep", "i");
  r[Symbol.match] = false;
  var r2 = RegExp(r, "g");
  assertFalse(r === r2);
  assertEquals(/biep/i, r);
  assertEquals(/biep/g, r2);
})();

(function() {
  class A extends RegExp {
    get ["constructor"]() { log.push("constructor"); return RegExp; }
  }
  var r = new A("biep");
  var log = [];
  var r2 = RegExp(r);
  assertEquals(["constructor"], log);
  assertTrue(r === r2);
})();
