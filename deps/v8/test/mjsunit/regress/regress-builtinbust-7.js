// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if ("Intl" in this) {
  function overflow() {
    return overflow() + 1;
  }
  Object.defineProperty = overflow;
  assertDoesNotThrow(function() { Intl.Collator.supportedLocalesOf("en"); });

  var date = new Date(Date.UTC(2004, 12, 25, 3, 0, 0));
  var options = {
    weekday: "long",
    year: "numeric",
    month: "long",
    day: "numeric"
  };

  Object.apply = overflow;
  assertDoesNotThrow(function() { date.toLocaleDateString("de-DE", options); });

  var options_incomplete = {};
  assertDoesNotThrow(function() {
    date.toLocaleDateString("de-DE", options_incomplete);
  });
  assertTrue(options_incomplete.hasOwnProperty("year"));

  assertDoesNotThrow(function() { date.toLocaleDateString("de-DE", undefined); });
  assertDoesNotThrow(function() { date.toLocaleDateString("de-DE"); });
  assertThrows(function() { date.toLocaleDateString("de-DE", null); }, TypeError);
}
