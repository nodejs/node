// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let displayNames = new Intl.DisplayNames(undefined, {type: 'language'});
// The default languageDisplay is 'dialect' if type is 'language'
assertEquals('dialect', displayNames.resolvedOptions().languageDisplay);

['region', 'script', 'calendar', 'dateTimeField', 'currency'].forEach(
    function(type) {
      let dn = new Intl.DisplayNames(undefined, {type});
      assertEquals(undefined, dn.resolvedOptions().languageDisplay);
    });

const styles = ["long", "short", "narrow"];
const types = ["calendar", "dateTimeField"];
const fallbacks = ["code", "none"];

styles.forEach(function(style) {
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

const languageDisplays = ["dialect", "standard"];
styles.forEach(function(style) {
  let type = 'language';
  languageDisplays.forEach(function(languageDisplay) {
    assertEquals(style,
        (new Intl.DisplayNames(['sr'], {style, type, languageDisplay}))
            .resolvedOptions().style);
    assertEquals(type,
        (new Intl.DisplayNames(['sr'], {style, type, languageDisplay}))
            .resolvedOptions().type);
    assertEquals(languageDisplay,
        (new Intl.DisplayNames(['sr'], {style, type, languageDisplay}))
            .resolvedOptions().languageDisplay);
  });
});
