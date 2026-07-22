// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("let a = new Intl.DateTimeFormat('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.NumberFormat('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.Collator('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.PluralRules('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.RelativeTimeFormat('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.ListFormat('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.DisplayNames('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.Segmenter('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
assertThrows("let a = new Intl.DurationFormat('de-u-22300-true-x-true')",
    RangeError, "Internal error. Icu error.");
