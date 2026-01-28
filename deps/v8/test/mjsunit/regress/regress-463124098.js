// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should execute without crashing

if (this.Temporal && this.Intl) {
  const md = new Temporal.PlainMonthDay(true, true);
  const format = Intl.DateTimeFormat("ast", {timeStyle: "short"});
  // This should throw due to a mismatched calendar
  assertThrows(() => format.format(md));
}
