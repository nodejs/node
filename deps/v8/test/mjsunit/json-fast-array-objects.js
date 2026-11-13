// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test homogeneous object arrays parse correctly via Direct Layout Allocation
// fast path.
(function TestHomogeneousObjectArray() {
  const items = [
    { id: 1, name: "foo", active: true, score: 95.5 },
    { id: 2, name: "bar", active: false, score: 88.25 },
    { id: 3, name: "baz", active: true, score: 100.0 }
  ];
  const json = JSON.stringify(items);
  const parsed = JSON.parse(json);
  assertEquals(items.length, parsed.length);
  for (let i = 0; i < items.length; i++) {
    assertEquals(items[i].id, parsed[i].id);
    assertEquals(items[i].name, parsed[i].name);
    assertEquals(items[i].active, parsed[i].active);
    assertEquals(items[i].score, parsed[i].score);
  }
})();

// Test heterogeneous/mismatched object arrays fall back correctly.
(function TestHeterogeneousObjectArray() {
  const items = [
    { id: 1, name: "foo" },
    { id: 2, name: "bar", extra: true },
    { id: 3, different: "key" }
  ];
  const json = JSON.stringify(items);
  const parsed = JSON.parse(json);
  assertEquals(items.length, parsed.length);
  assertEquals(items[0], parsed[0]);
  assertEquals(items[1], parsed[1]);
  assertEquals(items[2], parsed[2]);
})();

// Test reviver with homogeneous array.
(function TestReviverHomogeneousArray() {
  const items = [
    { a: 10, b: 20 },
    { a: 30, b: 40 }
  ];
  const json = JSON.stringify(items);
  const parsed = JSON.parse(json, (key, val) => {
    if (typeof val === "number") return val * 2;
    return val;
  });
  assertEquals(20, parsed[0].a);
  assertEquals(40, parsed[0].b);
  assertEquals(60, parsed[1].a);
  assertEquals(80, parsed[1].b);
})();

// Test that map transitions across child property parsing that deprecate
// feedback maps or normalize maps to dictionary mode do not trigger fast-path
// DCHECK/memory bugs.
(function TestDeprecatedMapSlowElementsTransition() {
  let arr = [];
  for (let i = 0; i < 1600; i++) {
    arr.push(`{"a":1.1,"p${i}":1}`);
  }
  let str = `[{"a":1,"b":1},{"a":1,"b":1},{"a":2,"b":[${arr.join(',')}]}]`;
  let res = JSON.parse(str);
  assertEquals(3, res.length);
  assertEquals(1, res[0].a);
  assertEquals(1, res[0].b);
  assertEquals(2, res[2].a);
  assertEquals(1600, res[2].b.length);
  assertEquals(1.1, res[2].b[0].a);
  assertEquals(1, res[2].b[0].p0);
  assertEquals(1.1, res[2].b[1599].a);
  assertEquals(1, res[2].b[1599].p1599);
})();

// Test all inline primitive value fast paths and fallbacks in kJsonFast
// (0, positive/negative Smis, >9 digit integers, -0, floats, exponents,
// empty/1-char/multi-char/escaped/two-byte strings, booleans, null, nested
// objects and arrays, and whitespace around ':', ',', and '}').
(function TestFastLoopValueTypesAndWhitespace() {
  // Keep field representations stable across elements 0 and 1 so element 1
  // transitions the DescriptorArray from kUnknown to kJsonFast without
  // reallocating descriptors during BuildJsonObject, ensuring elements 2 and 3
  // execute ParseFastJsonObjectProperties.
  const items = [
    {
      zero: 0,
      posSmi: 1,
      maxFastSmi: 2,
      negSmi: -1,
      doubleVal: 1.5,
      negDoubleVal: -1.5,
      zeroDoubleVal: 0.5,
      emptyStr: "init",
      oneChar: "i",
      multiChar: "init",
      escapedStr: "init",
      escapedQuoteStr: "init",
      boolT: true,
      boolF: false,
      nil: null,
      nestedObj: { x: 1, y: 2 },
      nestedArr: [1, 2]
    },
    {
      zero: 0,
      posSmi: 2,
      maxFastSmi: 3,
      negSmi: -2,
      doubleVal: 2.5,
      negDoubleVal: -2.5,
      zeroDoubleVal: 0.25,
      emptyStr: "warmup",
      oneChar: "w",
      multiChar: "warmup",
      escapedStr: "warmup",
      escapedQuoteStr: "warmup",
      boolT: true,
      boolF: false,
      nil: null,
      nestedObj: { x: 1, y: 2 },
      nestedArr: [1, 2]
    },
    {
      zero: 0,
      posSmi: 42,
      maxFastSmi: 999999999,
      negSmi: -123456789,
      doubleVal: 123.45,
      negDoubleVal: -37.5,
      zeroDoubleVal: 0.125,
      emptyStr: "",
      oneChar: "a",
      multiChar: "hello",
      escapedStr: "a\nb\"c\u0041",
      escapedQuoteStr: "\"a",
      boolT: true,
      boolF: false,
      nil: null,
      nestedObj: { x: 3, y: 4 },
      nestedArr: [1, 2, 3]
    },
    {
      zero: 0,
      posSmi: 1000000000,
      maxFastSmi: 1.5,
      negSmi: -1000000000,
      doubleVal: 1e5,
      negDoubleVal: -1e5,
      zeroDoubleVal: 0,
      emptyStr: "x",
      oneChar: "",
      multiChar: "λ",
      escapedStr: "plain",
      escapedQuoteStr: "\\",
      boolT: false,
      boolF: true,
      nil: null,
      nestedObj: {},
      nestedArr: []
    }
  ];

  // Compact 1-byte and 2-byte sources.
  const compact = JSON.stringify(items);
  assertEquals(items, JSON.parse(compact));

  // Pretty-printed with whitespace around ':', ',', and '}'.
  const spaced = JSON.stringify(items, null, 2);
  assertEquals(items, JSON.parse(spaced));

  // Force 2-byte source string with purely ASCII keys/values in kJsonFast,
  // and also test -0, 0.<digits>, and scientific notation in raw JSON text.
  // Elements 0 and 1 keep identical field representations (Double for 'a' and
  // 'c', Smi for 'b') so elements 2 and 3 run in kJsonFast.
  const twoByteJson =
      '[{"a":-0.5,"b":12,"c":-1.5,"d":"","e":"x","f":true,"g":false,' +
      '"h":null,"u":"☺"},' +
      '{"a":-0.25,"b":34,"c":-2.5,"d":"q","e":"","f":false,"g":true,' +
      '"h":null,"u":"☺"},' +
      '{"a":-0,"b":456,"c":-78,"d":"","e":"y","f":true,"g":false,' +
      '"h":null,"u":"☺"},' +
      '{"a":0,"b":789,"c":-1e2,"d":"z","e":"","f":false,"g":true,' +
      '"h":null,"u":"☺"}]';
  const parsedTwoByte = JSON.parse(twoByteJson);
  assertEquals(4, parsedTwoByte.length);
  assertEquals(-0, parsedTwoByte[2].a);
  assertEquals(456, parsedTwoByte[2].b);
  assertEquals(-78, parsedTwoByte[2].c);
  assertEquals("", parsedTwoByte[2].d);
  assertEquals("y", parsedTwoByte[2].e);
  assertEquals(0, parsedTwoByte[3].a);
  assertEquals(789, parsedTwoByte[3].b);
  assertEquals(-100, parsedTwoByte[3].c);

  // Test fewer properties than expected descriptor count (early '}') and
  // indexed properties interspersed in kJsonFast.
  const earlyClose = JSON.parse('[{"a":1,"b":2},{"a":1,"b":2},{"a":3}]');
  assertEquals(3, earlyClose[2].a);
  assertEquals(undefined, earlyClose[2].b);

  const withIndex = JSON.parse(
      '[{"a":1,"b":2},{"a":1,"b":2},{"a":3,"0":99,"b":4}]');
  assertEquals(3, withIndex[2].a);
  assertEquals(99, withIndex[2][0]);
  assertEquals(4, withIndex[2].b);

  // Test 2-byte JSON source in kJsonFast where BuildJsonObject falls off the
  // direct fast_keys_matched path and JSDataObjectBuilder's expected final map
  // fast path (e.g. first property generalizes from Smi to Double on element
  // 2), exercising MakeString on subsequent multi-character kJsonFast keys.
  const twoByteFallback = JSON.parse(
      '[{"firstKey":1,"secondKey":"☺"},{"firstKey":2,"secondKey":"☺"},' +
      '{"firstKey":3.5,"secondKey":"☺","extraKey":4}]');
  assertEquals(3.5, twoByteFallback[2].firstKey);
  assertEquals("☺", twoByteFallback[2].secondKey);
  assertEquals(4, twoByteFallback[2].extraKey);
})();

// Test syntax errors inside the kJsonFast loop preserve exact failure behavior.
(function TestFastLoopSyntaxErrors() {
  const prefix = '[{"a":1,"b":true},{"a":1,"b":true},';
  const badInputs = [
    `${prefix}{123:1,"b":true}]`,
    `${prefix}{"a" 1,"b":true}]`,
    `${prefix}{"a":`,
    `${prefix}{"a":   `,
    `${prefix}{"a":,"b":true}]`,
    `${prefix}{"a":01,"b":true}]`,
    `${prefix}{"a":-01,"b":true}]`,
    `${prefix}{"a":1.,"b":true}]`,
    `${prefix}{"a":-,"b":true}]`,
    `${prefix}{"a":tru,"b":true}]`,
    `${prefix}{"a":fals,"b":true}]`,
    `${prefix}{"a":nul,"b":true}]`,
    `${prefix}{"a":"\x01","b":true}]`,
    `${prefix}{"a":1 "b":true}]`,
    `${prefix}{"a":1,"b":true`,
  ];
  for (const input of badInputs) {
    assertThrows(() => JSON.parse(input), SyntaxError);
  }
})();
