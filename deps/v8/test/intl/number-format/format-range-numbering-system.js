// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-number-format-v3

let latn = new Intl.NumberFormat("en", {numberingSystem: "latn"})
assertDoesNotThrow(() => latn.formatRange(1, 234));

let arab = new Intl.NumberFormat("en", {numberingSystem: "arab"})
assertDoesNotThrow(() => arab.formatRange(1, 234));

let thai = new Intl.NumberFormat("en", {numberingSystem: "thai"})
assertDoesNotThrow(() => thai.formatRange(1, 234));
