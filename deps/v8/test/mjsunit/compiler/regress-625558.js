// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (var global = 0; global <= 256; global++) { }

function f() {
  global = "luft";
  global += ++global;
}

f();
