// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following numberingSystems are not supported yet
const algorithmicNumberingSystems = [
  "armn", "armnlow", "cyrl", "ethi", "geor", "grek", "greklow", "hans",
  "hansfin", "hant", "hantfin", "hebr", "jpan", "jpanfin", "roman", "romanlow",
  "taml"
];

algorithmicNumberingSystems.forEach(function(numberingSystem) {
  let df = new Intl.DateTimeFormat("en", {dateStyle: "full", numberingSystem});
  if (df.resolvedOptions().numberingSystem != numberingSystem) {
    assertEquals("latn", df.resolvedOptions().numberingSystem);
  }

  let df2 = new Intl.DateTimeFormat("en-u-nu-" + numberingSystem,
      {dateStyle: "full"});

  if (df2.resolvedOptions().numberingSystem != numberingSystem) {
    assertEquals("latn", df2.resolvedOptions().numberingSystem);
  }

  // Just verify it won't crash
  (new Date()).toLocaleString("en-u-nu-" + numberingSystem, {dateStyle: "full"});
});
