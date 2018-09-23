// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-do-expressions

function f() {
  print(
    do {
      for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }
    }
  );
}
f();
