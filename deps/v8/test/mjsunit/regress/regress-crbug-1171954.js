// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

// This causes the register used by the call in the later try-catch block to be
// used by the ToName conversion for null which causes a DCHECK fail when
// compiling. If register allocation changes, this test may no longer reproduce
// the crash but it is not easy write a proper test because it is linked to
// register allocation. This test should always work, so shouldn't cause any
// flakes.
try {
  var { [null]: __v_12, } = {};
} catch (e) {}

try {
  assertEquals((__v_40?.o?.m)().p);
} catch (e) {}
