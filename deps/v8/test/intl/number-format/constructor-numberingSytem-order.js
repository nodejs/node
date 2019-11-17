// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-add-calendar-numbering-system
const actual = [];

const options = {
  get localeMatcher() {
    actual.push("localeMatcher");
    return undefined;
  },
  get numberingSystem() {
    actual.push("numberingSystem");
    return undefined;
  },
  get style() {
    actual.push("style");
    return undefined;
  },
};

const expected = [
    "localeMatcher",
    "numberingSystem",
    "style"
];

let nf = new Intl.NumberFormat(undefined, options);
assertEquals(actual.join(":"), expected.join(":"));
