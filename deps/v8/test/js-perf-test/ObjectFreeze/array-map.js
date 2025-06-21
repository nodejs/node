// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupArrayMap(length) {
  var a = new Array(length);
  for (var i=0;i<length;i++) {
    a[i] = ''+i;
  }
  return Object.freeze(a);
}

const frozenArrayMap = setupArrayMap(200);

function driverArrayMap(n) {
  let result = 0;
  for (var i=0;i<n;i++) {
    result = frozenArrayMap.map(Number);
  }
  return result;
}

function ArrayMap() {
  driverArrayMap(1e3);
}

function ArrayMapWarmUp() {
  driverArrayMap(1e1);
  driverArrayMap(1e2);
}

createSuite('ArrayMap', 10, ArrayMap, ArrayMapWarmUp);
