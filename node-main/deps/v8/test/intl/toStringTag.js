// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let descriptor;

for (const [obj, tag] of
    [[Intl, "Intl"],
     [Intl.Collator.prototype, "Intl.Collator"],
     [Intl.DateTimeFormat.prototype, "Intl.DateTimeFormat"],
     [Intl.DisplayNames.prototype, "Intl.DisplayNames"],
     [Intl.Locale.prototype, "Intl.Locale"],
     [Intl.ListFormat.prototype, "Intl.ListFormat"],
     [Intl.NumberFormat.prototype, "Intl.NumberFormat"],
     [Intl.RelativeTimeFormat.prototype, "Intl.RelativeTimeFormat"],
     [Intl.PluralRules.prototype, "Intl.PluralRules"],
    ]) {
  descriptor = Object.getOwnPropertyDescriptor(obj,
                                               Symbol.toStringTag);
  assertEquals(tag, descriptor.value);
  assertFalse(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);
}
