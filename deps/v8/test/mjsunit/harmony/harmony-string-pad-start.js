// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-string-padding

(function TestMeta() {
  assertEquals(1, String.prototype.padStart.length);
  assertEquals("function", typeof String.prototype.padStart);
  assertEquals(Object.getPrototypeOf(Function),
               Object.getPrototypeOf(String.prototype.padStart));
  assertEquals("padStart", String.prototype.padStart.name);

  var desc = Object.getOwnPropertyDescriptor(String.prototype, "padStart");
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
  assertEquals(undefined, desc.get);
  assertEquals(undefined, desc.set);

  assertThrows(() => new Function(`${String.prototype.padStart}`), SyntaxError);
})();


(function TestRequireObjectCoercible() {
  var padStart = String.prototype.padStart;
  assertThrows(() => padStart.call(null, 4, "test"), TypeError);
  assertThrows(() => padStart.call(undefined, 4, "test"), TypeError);
  assertEquals("   123", padStart.call({
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
  assertEquals("   6.7", padStart.call(proxy, 6, " "));

  proxy = new Proxy({}, {
    get(t, name) {
      if (name === Symbol.toPrimitive || name === "valueOf") return;
      if (name === "toString") return () => 6.7;
      assertUnreachable();
    }
  });
  assertEquals("   6.7", padStart.call(proxy, 6, " "));
})();


(function TestToLength() {
  assertThrows(() => "123".padStart(Symbol("16")), TypeError);
  assertEquals("123", "123".padStart(-1));
  assertEquals("123", "123".padStart({ toString() { return -1; } }));
  assertEquals("123", "123".padStart(-0));
  assertEquals("123", "123".padStart({ toString() { return -0; } }));
  assertEquals("123", "123".padStart(+0));
  assertEquals("123", "123".padStart({ toString() { return +0; } }));
  assertEquals("123", "123".padStart(NaN));
  assertEquals("123", "123".padStart({ toString() { return NaN; } }));
})();


(function TestFillerToString() {
  assertEquals("         .", ".".padStart(10));
  assertEquals("         .", ".".padStart(10, undefined));
  assertEquals("nullnulln.", ".".padStart(10, null));
  assertEquals("XXXXXXXXX.", ".".padStart(10, { toString() { return "X"; } }));
  assertEquals(
      "111111111.",
      ".".padStart(10, { toString: undefined, valueOf() { return 1; } }));
})();


(function TestFillerEmptyString() {
  assertEquals(".", ".".padStart(10, ""));
  assertEquals(".", ".".padStart(10, { toString() { return ""; } }));
  assertEquals(
      ".", ".".padStart(10, { toString: undefined, valueOf() { return ""; } }));
})();


(function TestFillerRepetition() {
  for (var i = 2000; i > 0; --i) {
    var expected = "xoxo".repeat(i / 4).slice(0, i - 3) + "123";
    var actual = "123".padStart(i, "xoxo");
    assertEquals(expected, actual);
    assertEquals(i > "123".length ? i : 3, actual.length);
  }
})();


(function TestTruncation() {
  assertEquals("ba", "a".padStart(2, "bc"));
})();
