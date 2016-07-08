// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var largeArray = 'x'.repeat(999).split('');
var a = largeArray;

assertThrows(() => {
  for (;;) {
    a = a.concat(a, a, a, a, a, a);
  }}, RangeError);
