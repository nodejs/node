// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Array.prototype.getStaggeredFromMiddle = function(i) {
  if (i >= this.length) {
    throw("getStaggeredFromMiddle: OOB");
  }
  var middle = Math.floor(this.length / 2);
  var index = middle + (((i % 2) == 0) ? (i / 2) : (((1 - i) / 2) - 1));
  return this[index];
}

Array.prototype.contains = function(obj) {
  var i = this.length;
  while (i--) {
    if (this[i] === obj) {
      return true;
    }
  }
  return false;
}

Math.alignUp = function(raw, multiple) {
  return Math.floor((raw + multiple - 1) / multiple) * multiple;
}
