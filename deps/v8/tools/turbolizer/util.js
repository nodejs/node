// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function computeScrollTop(container, element) {
  const height = container.offsetHeight;
  const margin = Math.floor(height / 4);
  const pos = element.offsetTop;
  const currentScrollTop = container.scrollTop;
  if (pos < currentScrollTop + margin) {
    return Math.max(0, pos - margin);
  } else if (pos > (currentScrollTop + 3 * margin)) {
    return Math.max(0, pos - 3 * margin);
  }
  return pos;
}

class ViewElements {
  constructor(container) {
    this.container = container;
    this.scrollTop = undefined;
  }

  consider(element, doConsider) {
    if (!doConsider) return;
    const newScrollTop = computeScrollTop(this.container, element);
    if (isNaN(newScrollTop)) {
      console.log("NOO")
    }
    if (this.scrollTop === undefined) {
      this.scrollTop = newScrollTop;
    } else {
      this.scrollTop = Math.min(this.scrollTop, newScrollTop);
    }
  }

  apply(doApply) {
    if (!doApply || this.scrollTop === undefined) return;
    this.container.scrollTop = this.scrollTop;
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


function sortUnique(arr, f, equal) {
  if (arr.length == 0) return arr;
  arr = arr.sort(f);
  let ret = [arr[0]];
  for (var i = 1; i < arr.length; i++) {
    if (!equal(arr[i-1], arr[i])) {
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

function isIterable(obj) {
  return obj != null && obj != undefined
    && typeof obj != 'string' && typeof obj[Symbol.iterator] === 'function';
}
