// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var validStyle = ["full", "long", "medium", "short", undefined];
var invalidStyle = ["narrow", "numeric"];

validStyle.forEach(function(dateStyle) {
  validStyle.forEach(function(timeStyle) {
    assertDoesNotThrow(() =>
        new Intl.DateTimeFormat("en", {dateStyle, timeStyle}));
  });

  invalidStyle.forEach(function(timeStyle) {
    assertThrows(() =>
        new Intl.DateTimeFormat("en", {dateStyle, timeStyle}), RangeError);
  });
}
);

invalidStyle.forEach(function(dateStyle) {
  validStyle.forEach(function(timeStyle) {
    assertThrows(() =>
        new Intl.DateTimeFormat("en", {dateStyle, timeStyle}), RangeError);
  });
  invalidStyle.forEach(function(timeStyle) {
    assertThrows(() =>
        new Intl.DateTimeFormat("en", {dateStyle, timeStyle}), RangeError);
  });
}
);
