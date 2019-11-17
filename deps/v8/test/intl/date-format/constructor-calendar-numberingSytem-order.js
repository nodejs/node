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
  get calendar() {
    actual.push("calendar");
    return undefined;
  },
  get numberingSystem() {
    actual.push("numberingSystem");
    return undefined;
  },
  get hour12() {
    actual.push("hour12");
    return undefined;
  },
};

const expected = [
    "localeMatcher",
    "calendar",
    "numberingSystem",
    "hour12"
];

let df = new Intl.DateTimeFormat(undefined, options);
assertEquals(actual.join(":"), expected.join(":"));
