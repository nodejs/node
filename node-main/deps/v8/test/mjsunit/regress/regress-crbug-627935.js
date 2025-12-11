// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Intl) {
  assertThrows("Intl.DateTimeFormat('en-US', {timeZone: 0})", RangeError);
  assertThrows("Intl.DateTimeFormat('en-US', {timeZone: true})", RangeError);
  assertThrows("Intl.DateTimeFormat('en-US', {timeZone: null})", RangeError);

  var object = { toString: function() { return "UTC" } };
  assertDoesNotThrow("Intl.DateTimeFormat('en-US', {timeZone: object})");
}
