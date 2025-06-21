// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[
  // Redundant tag with preferred value.
  ["sgn-de", "gsg"],
  ["sgn-de-u-co-phonebk", "gsg-u-co-phonebk"],

  // Matching should be case-insensitive.
  ["sgn-De", "gsg"],

  // deprecated region tag
  ["und-Latn-dd", "und-Latn-DE"],
  ["und-dd-u-co-phonebk", "und-DE-u-co-phonebk"],
  ["de-dd-u-co-phonebk", "de-DE-u-co-phonebk"],
  ["de-latn-dd-u-co-phonebk", "de-Latn-DE-u-co-phonebk"],
  ["fr-ZR", "fr-CD"],

  // Deprecated [23]-letter language tags
  ["in", "id"],
  ["in-latn", "id-Latn"],
  ["in-latn-id", "id-Latn-ID"],
  ["in-latn-id-u-ca-gregory", "id-Latn-ID-u-ca-gregory"],
  ["jw", "jv"],
  ["aam", "aas"],
  ["aam-u-ca-gregory", "aas-u-ca-gregory"],
].forEach(([inputLocale, expectedLocale]) => {
  const canonicalLocales = Intl.getCanonicalLocales(inputLocale);
  assertEquals(1, canonicalLocales.length);
  assertEquals(expectedLocale, canonicalLocales[0]);
})
