// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testStringRawArity() {
  assertEquals(1, String.raw.length);
})();


(function testStringRawCallSiteToObject() {
  assertThrows("String.raw()", TypeError);
})();


(function testStringRawCallSiteRawToObject() {
  assertThrows("String.raw([])", TypeError);
})();


(function testStringRawUndefinedLength() {
  var callSiteObj = [];
  callSiteObj.raw = {};
  assertEquals("", String.raw(callSiteObj));

  callSiteObj.raw = { lengt: 0 };
  assertEquals("", String.raw(callSiteObj));
})();


(function testStringRawZeroLength() {
  var callSiteObj = [];
  callSiteObj.raw = { length: 0 };
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));

  callSiteObj.raw = [];
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));
})();


(function testStringRawNegativeLength() {
  var callSiteObj = [];
  callSiteObj.raw = { length: -85 };
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));

  callSiteObj.raw = [];
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));
})();


(function testStringRawNaNLength() {
  var callSiteObj = [];
  callSiteObj.raw = { length: NaN };
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));

  callSiteObj.raw = [];
  assertEquals("", String.raw(callSiteObj));
  assertEquals("", String.raw(callSiteObj, "a", "b", "c"));
})();


(function testStringRawBasic() {
  var callSiteObj = [];
  callSiteObj.raw = ["a"];
  assertEquals("a", String.raw(callSiteObj));
})();


(function testStringRawNoSubst() {
  var callSiteObj = [];
  callSiteObj.raw = ["a", "b"];
  assertEquals("ab", String.raw(callSiteObj));
})();


(function testStringRawSubst() {
  var callSiteObj = [];
  callSiteObj.raw = ["a", "b"];
  assertEquals("a!b", String.raw(callSiteObj, "!"));

  callSiteObj.raw = ["a", "b", "c"];
  assertEquals("abc", String.raw(callSiteObj));

  callSiteObj.raw = ["a", "b", "c"];
  assertEquals("a!bc", String.raw(callSiteObj, "!"));

  callSiteObj.raw = ["a", "b", "c"];
  assertEquals("a!b?c", String.raw(callSiteObj, "!", "?"));

  callSiteObj.raw = ["\n", "\r\n", "\r"];
  assertEquals("\nx\r\ny\r", String.raw(callSiteObj, "x", "y"));

  callSiteObj.raw = ["\n", "\r\n", "\r"];
  assertEquals("\n\r\r\r\n\n\r", String.raw(callSiteObj, "\r\r", "\n"));
})();


(function testStringRawArrayLikeSubst() {
  var callSiteObj = [];
  callSiteObj.raw = {"length": 2, "0": "a", "1": "b", "2": "c"};
  assertEquals("axb", String.raw(callSiteObj, "x", "y"));

  callSiteObj.raw = {"length": 4, "0": "a", "1": "b", "2": "c"};
  assertEquals("axbycundefined", String.raw(callSiteObj, "x", "y"));
})();


(function testStringRawAccessors() {
  var callSiteObj = {};
  callSiteObj.raw = {};
  Object.defineProperties(callSiteObj, {
    "length": {
      get: function() { assertUnreachable(); },
      set: function(v) { assertUnreachable(); }
    },
    "0": {
      get: function() { assertUnreachable(); },
      set: function(v) { assertUnreachable(); }
    },
    "1": {
      get: function() { assertUnreachable(); },
      set: function(v) { assertUnreachable(); }
    }
  });
  Object.defineProperties(callSiteObj.raw, {
    "length": {
      get: function() { return 2; },
      set: function(v) { assertUnreachable(); }
    },
    "0": {
      get: function() { return "getter values"; },
      set: function(v) { assertUnreachable(); }
    },
    "1": {
      get: function() { return "are nice"; },
      set: function(v) { assertUnreachable(); }
    }
  });
  assertEquals("getter values are nice", String.raw(callSiteObj, " "));
})();


(function testStringRawHoleyArray() {
  var callSiteObj = [];
  callSiteObj.raw = ["1."];
  callSiteObj.raw[2] = ".2";
  assertEquals("1.undefined.2", String.raw(callSiteObj));
})();


(function testStringRawAccessorThrows() {
  var callSiteObj = [];
  callSiteObj.raw = [1];
  function MyError() {}
  Object.defineProperty(callSiteObj.raw, "0", {
    get: function() { throw new MyError(); }
  });
  assertThrows(function() { String.raw(callSiteObj); }, MyError);
})();


(function testStringRawToStringSafe() {
  var callSiteObj = [];
  callSiteObj.raw = [null, undefined, 1, "str", true, false, NaN, Infinity, {}];
  assertEquals("nullundefined1strtruefalseNaNInfinity[object Object]",
               String.raw(callSiteObj));

  callSiteObj.raw = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"];
  assertEquals("0null1undefined213str4true5false6NaN7Infinity8[object Object]9",
               String.raw(callSiteObj, null, void 0, 1, "str", true, false,
               NaN, Infinity, {}));
})();


(function testStringRawToStringSymbolThrows() {
  var callSiteObj = [];
  callSiteObj.raw = [Symbol("foo")];
  assertThrows(function() {
    String.raw(callSiteObj);
  }, TypeError);

  callSiteObj.raw = ["1", "2"];
  assertThrows(function() {
    String.raw(callSiteObj, Symbol("foo"));
  }, TypeError);
})();


(function testStringRawToStringThrows() {
  var callSiteObj = [];
  var thrower = {};
  function MyError() {}
  thrower.toString = function() {
    throw new MyError();
  }

  callSiteObj.raw = [thrower];
  assertThrows(function() {
    String.raw(callSiteObj);
  }, MyError);

  callSiteObj.raw = ["1", "2"];
  assertThrows(function() {
    String.raw(callSiteObj, thrower);
  }, MyError);
})();


(function testStringRawToStringValueOfThrows() {
  var callSiteObj = [];
  var thrower = {};
  function MyError() {}
  thrower.toString = null;
  thrower.valueOf = function() {
    throw new MyError();
  }

  callSiteObj.raw = [thrower];
  assertThrows(function() {
    String.raw(callSiteObj);
  }, MyError);

  callSiteObj.raw = ["1", "2"];
  assertThrows(function() {
    String.raw(callSiteObj, thrower);
  }, MyError);
})();


(function testStringRawOrder() {
  var order = [];
  var callSiteObj = [];
  callSiteObj.raw = {};
  function arg(v) {
    var result = {};
    result.toString = null;
    result.valueOf = function() { order.push("arg" + v); return v; }
    return result;
  }

  Object.defineProperty(callSiteObj.raw, "length", {
    get: function() { order.push("length"); return 3; }
  });
  [1, 3, 5].forEach(function(v, i) {
    Object.defineProperty(callSiteObj.raw, i, {
      get: function() { order.push("raw" + v); return v; }
    });
  });

  assertEquals("12345", String.raw(callSiteObj, arg(2), arg(4), arg(6)));
  assertEquals(["length", "raw1", "arg2", "raw3", "arg4", "raw5"], order);
})();


(function testStringRawToStringSubstitutionsOrder() {
  var subs = [];
  var log = [];
  function stringify(toString) {
    var valueOf = "_" + toString + "_";
    return {
      toString: function() { return toString; },
      valueOf: function() { return valueOf; }
    };
  }
  function getter(name, value) {
    return {
      get: function() {
        log.push("get" + name);
        return value;
      },
      set: function(v) {
        log.push("set" + name);
      }
    };
  }
  Object.defineProperties(subs, {
    0: getter(0, stringify("a")),
    1: getter(1, stringify("b")),
    2: getter(2, stringify("c"))
  });

  assertEquals("-a-b-c-", String.raw`-${subs[0]}-${subs[1]}-${subs[2]}-`);
  assertArrayEquals(["get0", "get1", "get2"], log);

  log.length = 0;
  assertEquals("-a-", String.raw`-${subs[0]}-`);
  assertArrayEquals(["get0"], log);
})();
