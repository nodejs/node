// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test invalid timezones under US/...  won't cause crash.
assertThrows(() => {
  new Intl.DateTimeFormat("en" , { timeZone: "US/Alaska0" });},
  RangeError);
