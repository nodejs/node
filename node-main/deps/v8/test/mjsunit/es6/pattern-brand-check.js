// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createNonRegExp(calls) {
  return {
    get [Symbol.match]() {
      calls.push("@@match");
      return undefined;
    },
    get [Symbol.replace]() {
      calls.push("@@replace");
      return undefined;
    },
    get [Symbol.search]() {
      calls.push("@@search");
      return undefined;
    },
    get [Symbol.split]() {
      calls.push("@@split");
      return undefined;
    },
    [Symbol.toPrimitive]() {
      calls.push("@@toPrimitive");
      return "";
    }
  };
}

(function testStringMatchBrandCheck() {
  var calls = [];
  "".match(createNonRegExp(calls));
  assertEquals(["@@match", "@@toPrimitive"], calls);
})();

(function testStringSearchBrandCheck() {
  var calls = [];
  "".search(createNonRegExp(calls));
  assertEquals(["@@search", "@@toPrimitive"], calls);
})();

(function testStringSplitBrandCheck() {
  var calls = [];
  "".split(createNonRegExp(calls));
  assertEquals(["@@split", "@@toPrimitive"], calls);
})();

(function testStringReplaceBrandCheck() {
  var calls = [];
  "".replace(createNonRegExp(calls), "");
  assertEquals(["@@replace", "@@toPrimitive"], calls);
})();
