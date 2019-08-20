// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupArray(length) {
  var a = new Array(length);
  for (var i=0;i<length;i++) {
    a[i] = ''+i;
  }
  return Object.freeze(a);
}

const frozenArray = setupArray(200);

function driverArrayIndexOf(n) {
  let result = 0;
  for (var i=0;i<n;i++) {
    result += frozenArray.indexOf(''+i)==-1?0:1;
  }
  return result;
}

function ArrayIndexOf() {
  driverArrayIndexOf(1e4);
}

function ArrayIndexOfWarmUp() {
  driverArrayIndexOf(1e1);
  driverArrayIndexOf(1e2);
  driverArrayIndexOf(1e3);
}

createSuite('ArrayIndexOf', 10, ArrayIndexOf, ArrayIndexOfWarmUp);

function driverArrayIncludes(n) {
  let result = 0;
  for (var i=0;i<n;i++) {
    result += frozenArray.includes(''+i)?0:1;
  }
  return result;
}

function ArrayIncludes() {
  driverArrayIncludes(1e4);
}

function ArrayIncludesWarmUp() {
  driverArrayIncludes(1e1);
  driverArrayIncludes(1e2);
  driverArrayIncludes(1e3);
}

createSuite('ArrayIncludes', 10, ArrayIncludes, ArrayIncludesWarmUp);
