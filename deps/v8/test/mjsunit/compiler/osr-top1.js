// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr --allow-natives-syntax

var sum = 0;
for (var i = 0; i < 10000; i++) {
  if (i == 100) %OptimizeOsr();
  var x = i + 2;
  var y = x + 5;
  var z = y + 3;
  sum += z;
}

assertEquals(50095000, sum);
