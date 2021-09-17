// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-analyze-environment-liveness
// Flags: --interrupt-budget=100

var val = {};
try {
  arr = [{}, [], {}];
  for (var i in arr) {
    for (var val = 0; val < 100; val++) {
    }
  }
} catch(e) { "Caught: " + e; }
