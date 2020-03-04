// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-displaynames

let displayNames = new Intl.DisplayNames();
// The default style is 'long'
assertEquals('long', displayNames.resolvedOptions().style);

// The default type is 'language'
assertEquals('language', displayNames.resolvedOptions().type);

// The default fallback is 'code'
assertEquals('code', displayNames.resolvedOptions().fallback);

const styles = ["long", "short", "narrow"];
const types = ["language", "region", "script", "currency", "weekday", "month",
      "quarter", "dayPeriod", "dateTimeField"];
const fallbacks = ["code", "none"];

styles.forEach(function(style) {
  assertEquals(style,
      (new Intl.DisplayNames(['sr'], {style})).resolvedOptions().style);
  assertEquals(types[0],
      (new Intl.DisplayNames(['sr'], {style})).resolvedOptions().type);
  assertEquals(fallbacks[0],
      (new Intl.DisplayNames(['sr'], {style})).resolvedOptions().fallback);
  types.forEach(function(type) {
    assertEquals(style,
        (new Intl.DisplayNames(['sr'], {style, type})).resolvedOptions().style);
    assertEquals(type,
        (new Intl.DisplayNames(['sr'], {style, type})).resolvedOptions().type);
    assertEquals(fallbacks[0],
        (new Intl.DisplayNames(
            ['sr'], {style, type})).resolvedOptions().fallback);
    fallbacks.forEach(function(fallback) {
      assertEquals(style,
          (new Intl.DisplayNames(
              ['sr'], {style, type, fallback})).resolvedOptions().style);
      assertEquals(type,
          (new Intl.DisplayNames(
              ['sr'], {style, type, fallback})).resolvedOptions().type);
      assertEquals(fallback,
          (new Intl.DisplayNames(
              ['sr'], {style, type, fallback})).resolvedOptions().fallback);
    });
  });
});
