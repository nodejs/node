// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome filter out data of algorithm numberingSystems so we need to test none
// of them got returned.
let name = "numberingSystem";
let items = Intl.supportedValuesOf(name);

function verifyNoAlgorithm(nu) {
  assertTrue(items.indexOf(nu) < 0, "should not return '" + nu + "' which is algorithmic");
}

["armn", "armnlow", "cyrl", "ethi", "finance", "geor", "grek", "greklow",
    "hans", "hansfin", "hant", "hantfin", "hebr", "japn", "japnfin",
    "roman", "romanlow", "taml", "traditio"].forEach(function(nu) {
  verifyNoAlgorithm(nu);
});
