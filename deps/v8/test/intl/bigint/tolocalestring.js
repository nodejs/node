// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var locales = [
    "en",  // "1,234,567,890,123,456"
    "de",  // "1.234.567.890.123.456"
    "fr",  // "1 234 567 890 123 456"
    "hi",  // "1,23,45,67,89,01,23,456"
    "fa",  // "۱٬۲۳۴٬۵۶۷٬۸۹۰٬۱۲۳٬۴۵۶"
    "th-u-nu-thai",  // "๑,๒๓๔,๕๖๗,๘๙๐,๑๒๓,๔๕๖"
];

var data = [
    Number.MAX_SAFE_INTEGER,
    -Number.MAX_SAFE_INTEGER,
    Math.floor(Number.MAX_SAFE_INTEGER / 2),
    0,
    /// -0, // this case is broken now.
];

for (var locale of locales) {
  let nf = new Intl.NumberFormat(locale);

  let percentOption = {style: "percent"};
  let nfPercent = new Intl.NumberFormat(locale, percentOption);
  for (var n of data) {
    let bigint = BigInt(n);
    // Test NumberFormat w/ number output the same as
    // BigInt.prototype.toLocaleString()
    assertEquals(nf.format(n), bigint.toLocaleString(locale));

    // Test NumberFormat output the same regardless pass in as number or BigInt
    assertEquals(nf.format(n), nf.format(bigint));

    // Test formatToParts
    assertEquals(nf.formatToParts(n), nf.formatToParts(bigint));

    // Test output with option
    // Test NumberFormat w/ number output the same as
    // BigInt.prototype.toLocaleString()
    assertEquals(nfPercent.format(n),
        bigint.toLocaleString(locale, percentOption));

    // Test NumberFormat output the same regardless pass in as number or BigInt
    assertEquals(nfPercent.format(n), nfPercent.format(bigint));
    assertEquals(nfPercent.formatToParts(n), nfPercent.formatToParts(bigint));
  }

  // Test very big BigInt
  let veryBigInt = BigInt(Number.MAX_SAFE_INTEGER) *
      BigInt(Number.MAX_SAFE_INTEGER) *
      BigInt(Number.MAX_SAFE_INTEGER);
  assertEquals(nf.format(veryBigInt), veryBigInt.toLocaleString(locale));
  // It should output different than toString
  assertFalse(veryBigInt.toLocaleString(locale) == veryBigInt.toString());
  assertTrue(veryBigInt.toLocaleString(locale).length >
      veryBigInt.toString().length);
}
