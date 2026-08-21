// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

var __v_27278 = "x";
for (var __v_27279 = 0; __v_27279 != 13; __v_27279++) {
  try { __v_27278 += __v_27278; } catch (e) {}
}

// Can throw or not, but should not crash.
try { /(xx|x)*/.exec(__v_27278); } catch (e) {}
