// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --always-opt --turbo-filter=*

function assertEquals() {}

function f(o) {
  if (o.setterProperty = 0) {
    return 1;
  }
  return 2;
}

function deopt() { %DeoptimizeFunction(f); }

assertEquals(2,
             f(Object.defineProperty({}, "setterProperty", { set: deopt })));
