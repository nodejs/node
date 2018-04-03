// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: #sec-intl.numberformat.prototype.formattoparts
description: Intl.NumberFormat.prototype.formatToParts called with no parameters
info: >
  Intl.NumberFormat.prototype.formatToParts ([ value ])

  3. If value is not provided, let value be undefined.
---*/

var nf = new Intl.NumberFormat();

// Example value: [{"type":"nan","value":"NaN"}]
var implicit = nf.formatToParts();
var explicit = nf.formatToParts(undefined);

assert(partsEquals(implicit, explicit),
  "formatToParts() should be equivalent to formatToParts(undefined)");

function partsEquals(parts1, parts2) {
  if (parts1.length !== parts2.length) return false;
  for (var i = 0; i < parts1.length; i++) {
    var part1 = parts1[i];
    var part2 = parts2[i];
    if (part1.type !== part2.type) return false;
    if (part1.value !== part2.value) return false;
  }
  return true;
}
