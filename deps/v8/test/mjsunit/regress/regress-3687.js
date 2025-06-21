// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var t1 = { f1: 0 };
var t2 = { f2: 0 };

var z = {
  x: {
    x: t1,
    y: {
      x: {},
      z1: {
        x: t2,
        y: 1
      }
    }
  },
  z2: 0
};
