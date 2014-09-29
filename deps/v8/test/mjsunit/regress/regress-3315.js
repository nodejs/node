// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var indexZeroCallCount = 0;
var indexOneCallCount = 0;
var lengthCallCount = 0;
var acceptList = {
  get 0() {
    indexZeroCallCount++;
    return 'foo';
  },
  get 1() {
    indexOneCallCount++;
    return 'bar';
  },
  get length() {
    lengthCallCount++;
    return 1;
  }
};

Object.observe({}, function(){}, acceptList);
assertEquals(1, lengthCallCount);
assertEquals(1, indexZeroCallCount);
assertEquals(0, indexOneCallCount);
