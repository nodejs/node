// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  const nf = new Intl.NumberFormat('bn-u-nu-mathsanb', {
    style: 'currency',
    currency: 'USD',
    minimumSignificantDigits: 1,
  });

  const huge_bigint = -(1n << 4000000n);
  assertThrows(() => nf.format(huge_bigint), TypeError);
  assertThrows(() => nf.formatToParts(huge_bigint), TypeError);
  assertThrows(() => nf.formatRange(huge_bigint, huge_bigint), TypeError);
  assertThrows(
      () => nf.formatRangeToParts(huge_bigint, huge_bigint), TypeError);
  assertThrows(() => huge_bigint.toLocaleString('bn-u-nu-mathsanb'), TypeError);

  const huge_str = '1.' + '0'.repeat(1000000);
  assertThrows(() => nf.format(huge_str), TypeError);
  assertThrows(() => nf.formatRange(huge_str, huge_str), TypeError);

  const sci_nf = new Intl.NumberFormat('en', {notation: 'scientific'});
  assertEquals('1E-400', sci_nf.format('1e-400'));
}
