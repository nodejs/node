// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestMeta() {
  assertEquals(1, String.prototype.padEnd.length);
  assertEquals("function", typeof String.prototype.padEnd);
  assertEquals(Object.getPrototypeOf(Function),
               Object.getPrototypeOf(String.prototype.padEnd));
  assertEquals("padEnd", String.prototype.padEnd.name);

  var desc = Object.getOwnPropertyDescriptor(String.prototype, "padEnd");
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
  assertEquals(undefined, desc.get);
  assertEquals(undefined, desc.set);

  assertThrows(() => new Function(`${String.prototype.padEnd}`), SyntaxError);
})();


(function TestRequireObjectCoercible() {
  var padEnd = String.prototype.padEnd;
  assertThrows(() => padEnd.call(null, 4, "test"), TypeError);
  assertThrows(() => padEnd.call(undefined, 4, "test"), TypeError);
  assertEquals("123   ", padEnd.call({
    __proto__: null,
    valueOf() { return 123; }
  }, 6, " "));

  var proxy = new Proxy({}, {
    get(t, name) {
      if (name === Symbol.toPrimitive || name === "toString") return;
      if (name === "valueOf") return () => 6.7;
      assertUnreachable();
    }
  });
  assertEquals("6.7   ", padEnd.call(proxy, 6, " "));

  proxy = new Proxy({}, {
    get(t, name) {
      if (name === Symbol.toPrimitive || name === "valueOf") return;
      if (name === "toString") return () => 6.7;
      assertUnreachable();
    }
  });
  assertEquals("6.7   ", padEnd.call(proxy, 6, " "));
})();


(function TestToLength() {
  assertThrows(() => "123".padEnd(Symbol("16")), TypeError);
  assertEquals("123", "123".padEnd(-1));
  assertEquals("123", "123".padEnd({ toString() { return -1; } }));
  assertEquals("123", "123".padEnd(-0));
  assertEquals("123", "123".padEnd({ toString() { return -0; } }));
  assertEquals("123", "123".padEnd(+0));
  assertEquals("123", "123".padEnd({ toString() { return +0; } }));
  assertEquals("123", "123".padEnd(NaN));
  assertEquals("123", "123".padEnd({ toString() { return NaN; } }));
})();


(function TestFillerToString() {
  assertEquals(".         ", ".".padEnd(10));
  assertEquals(".         ", ".".padEnd(10, undefined));
  assertEquals(".nullnulln", ".".padEnd(10, null));
  assertEquals(".XXXXXXXXX", ".".padEnd(10, { toString() { return "X"; } }));
  assertEquals(
      ".111111111",
      ".".padEnd(10, { toString: undefined, valueOf() { return 1; } }));
})();


(function TestFillerEmptyString() {
  assertEquals(".", ".".padEnd(10, ""));
  assertEquals(".", ".".padEnd(10, { toString() { return ""; } }));
  assertEquals(
      ".", ".".padEnd(10, { toString: undefined, valueOf() { return ""; } }));
})();


(function TestFillerRepetition() {
  for (var i = 2000; i > 0; --i) {
    var expected = "123" + "xoxo".repeat(i / 4).slice(0, i - 3);
    var actual = "123".padEnd(i, "xoxo");
    assertEquals(expected, actual);
    assertEquals(i > "123".length ? i : 3, actual.length);
  }
})();


(function TestTruncation() {
  assertEquals("ab", "a".padEnd(2, "bc"));
})();


(function TestMaxLength() {
  assertThrows(() => "123".padEnd(Math.pow(2, 40)), RangeError);
  assertThrows(() => "123".padEnd(1 << 30), RangeError);
})();


(function TestNoArguments() {
  assertEquals("abc", "abc".padEnd());
})();
