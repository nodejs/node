// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const testData = [
    ["short"],
    ["long"],
];

for (const [compactDisplay] of testData) {
  nf = new Intl.NumberFormat("en", {compactDisplay, notation: "compact"});
  assertEquals(compactDisplay, nf.resolvedOptions().compactDisplay);
}
