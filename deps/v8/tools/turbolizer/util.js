// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function makeContainerPosVisible(container, pos) {
  var height = container.offsetHeight;
  var margin = Math.floor(height / 4);
  if (pos < container.scrollTop + margin) {
    pos -= margin;
    if (pos < 0) pos = 0;
    container.scrollTop = pos;
    return;
  }
  if (pos > (container.scrollTop + 3 * margin)) {
    pos = pos - 3 * margin;
    if (pos < 0) pos = 0;
    container.scrollTop = pos;
  }
}


function lowerBound(a, value, compare, lookup) {
  let first = 0;
  let count = a.length;
  while (count > 0) {
    let step = Math.floor(count / 2);
    let middle = first + step;
    let middle_value = (lookup === undefined) ? a[middle] : lookup(a, middle);
    let result = (compare === undefined) ? (middle_value < value) : compare(middle_value, value);
    if (result) {
      first = middle + 1;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  return first;
}


function upperBound(a, value, compare, lookup) {
  let first = 0;
  let count = a.length;
  while (count > 0) {
    let step = Math.floor(count / 2);
    let middle = first + step;
    let middle_value = (lookup === undefined) ? a[middle] : lookup(a, middle);
    let result = (compare === undefined) ? (value < middle_value) : compare(value, middle_value);
    if (!result) {
      first = middle + 1;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  return first;
}


function sortUnique(arr, f) {
  arr = arr.sort(f);
  let ret = [arr[0]];
  for (var i = 1; i < arr.length; i++) {
    if (arr[i-1] !== arr[i]) {
      ret.push(arr[i]);
    }
  }
  return ret;
}

// Partial application without binding the receiver
function partial(f) {
  var arguments1 = Array.prototype.slice.call(arguments, 1);
  return function() {
    var arguments2 = Array.from(arguments);
    f.apply(this, arguments1.concat(arguments2));
  }
}
