// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-number-format-v3

const validRanges = [[-12345, -5678], [-12345, 56789], [12345, 56789]];

const nf = new Intl.NumberFormat("en", {signDisplay: "exceptZero"});
['formatRange', 'formatRangeToParts'].forEach(function(method) {
  assertEquals("function", typeof nf[method]);

  // 2. Perform ? RequireInternalSlot(nf, [[InitializedNumberFormat]]).
  // Assert if called without nf
  let f = nf[method];
  assertThrows(() => { f(1, 23) }, TypeError);

  // Assert normal call success
  assertDoesNotThrow(() => nf[method](1, 23));

  // 3. If start is undefined ..., throw a TypeError exception.
  assertThrows(() => { nf[method](undefined, 23) }, TypeError);
  // 3. If ... end is undefined, throw a TypeError exception.
  assertThrows(() => { nf[method](1, undefined) }, TypeError);

  // 4. Let x be ? ToNumeric(start).
  // Verify it won't throw error
  assertDoesNotThrow(() => nf[method](null, 23));
  assertDoesNotThrow(() => nf[method](false, 23));
  assertDoesNotThrow(() => nf[method](true, 23));
  assertDoesNotThrow(() => nf[method](12, 23));
  assertDoesNotThrow(() => nf[method](12n, 23));
  // Verify it will throw error
  assertThrows(() => { nf[method](Symbol(12), 23) }, TypeError);

  // 5. Let y be ? ToNumeric(end).
  // Verify it won't throw error
  assertDoesNotThrow(() => nf[method](-12, null));
  assertDoesNotThrow(() => nf[method](-12, false));
  assertDoesNotThrow(() => nf[method](-12, true));
  assertDoesNotThrow(() => nf[method](12, 23));
  assertDoesNotThrow(() => nf[method](12, 23n));

  // Verify it will throw error
  assertThrows(() => { nf[method](12, Symbol(23)) }, TypeError);

  // 6. If x is NaN ..., throw a RangeError exception.
  assertThrows(() => { nf[method](NaN, 23) }, RangeError);

  // 6. If ... y is NaN, throw a RangeError exception.
  assertThrows(() => { nf[method](12, NaN) }, RangeError);

  // 8. If x is greater than y, throw a RangeError exception.
  // neither x nor y are bigint.
  assertThrows(() => { nf[method](23, 12) }, RangeError);
  assertDoesNotThrow(() => nf[method](12, 23));
  // x is not bigint but y is.
  assertThrows(() => { nf[method](23, 12n) }, RangeError);
  assertDoesNotThrow(() => nf[method](12, 23n));
  // x is bigint but y is not.
  assertThrows(() => { nf[method](23n, 12) }, RangeError);
  assertDoesNotThrow(() => nf[method](12n, 23));
  // both x and y are bigint.
  assertThrows(() => { nf[method](23n, 12n) }, RangeError);
  assertDoesNotThrow(() => nf[method](12n, 23n));

  validRanges.forEach(
      function([x, y]) {
        const X = BigInt(x);
        const Y = BigInt(y);
        const formatted_x_y = nf[method](x, y);
        const formatted_X_y = nf[method](X, y);
        const formatted_x_Y = nf[method](x, Y);
        const formatted_X_Y = nf[method](X, Y);
        assertEquals(formatted_x_y, formatted_X_y);
        assertEquals(formatted_x_y, formatted_x_Y);
        assertEquals(formatted_x_y, formatted_X_Y);

    });
});

// Check the number of part with type: "plusSign" and "minusSign" are corre
validRanges.forEach(
    function([x, y]) {
      const expectedPlus = (x > 0) ? ((y > 0) ? 2 : 1) : ((y > 0) ? 1 : 0);
      const expectedMinus = (x < 0) ? ((y < 0) ? 2 : 1) : ((y < 0) ? 1 : 0);
      let actualPlus = 0;
      let actualMinus = 0;
      const parts = nf.formatRangeToParts(x, y);
      parts.forEach(function(part) {
        if (part.type == "plusSign") actualPlus++;
        if (part.type == "minusSign") actualMinus++;
      });
      const method = "formatRangeToParts(" + x + ", " + y + "): ";
      assertEquals(expectedPlus, actualPlus,
          method + "Number of type: 'plusSign' in parts is incorrect");
      assertEquals(expectedMinus, actualMinus,
          method + "Number of type: 'minusSign' in parts is incorrect");
    });

// From https://github.com/tc39/proposal-intl-numberformat-v3#formatrange-ecma-402-393
const nf2 = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "EUR",
  maximumFractionDigits: 0,
});

// README.md said it expect "€3–5"
assertEquals("€3 – €5", nf2.formatRange(3, 5));

const nf3 = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "EUR",
  maximumFractionDigits: 0,
});
const actual3 = nf3.formatRangeToParts(3, 5);
/*
[
  {type: "currency", value: "€", source: "startRange"}
  {type: "integer", value: "3", source: "startRange"}
  {type: "literal", value: "–", source: "shared"}
  {type: "integer", value: "5", source: "endRange"}
]
*/
assertEquals(5, actual3.length);
assertEquals("currency", actual3[0].type);
assertEquals("€", actual3[0].value);
assertEquals("startRange", actual3[0].source);
assertEquals("integer", actual3[1].type);
assertEquals("3", actual3[1].value);
assertEquals("startRange", actual3[1].source);
assertEquals("literal", actual3[2].type);
assertEquals(" – ", actual3[2].value);
assertEquals("shared", actual3[2].source);
assertEquals("currency", actual3[3].type);
assertEquals("€", actual3[3].value);
assertEquals("endRange", actual3[3].source);
assertEquals("integer", actual3[4].type);
assertEquals("5", actual3[4].value);
assertEquals("endRange", actual3[4].source);

const nf4 = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "EUR",
  maximumFractionDigits: 0,
});
assertEquals("~€3", nf4.formatRange(2.9, 3.1));

const nf5 = new Intl.NumberFormat("en-US", {
  style: "currency",
  currency: "EUR",
  signDisplay: "always",
});
assertEquals("~+€3.00", nf5.formatRange(2.999, 3.001));

const nf6 = new Intl.NumberFormat("en");
assertEquals("3–∞", nf6.formatRange(3, 1/0));
assertThrows(() => { nf6.formatRange(3, 0/0); }, RangeError);
