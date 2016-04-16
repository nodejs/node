// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-object-observe

var records;
function observer(r) {
  records = r;
}

Object.defineProperty(Array.prototype, '0', {
  get: function() { return 0; },
  set: function() { throw "boom!"; }
});
arr = [1, 2];
Array.observe(arr, observer);
arr.length = 0;
assertEquals(0, arr.length);

Object.deliverChangeRecords(observer);
assertEquals(1, records.length);
assertEquals('splice', records[0].type);
assertArrayEquals([1, 2], records[0].removed);
