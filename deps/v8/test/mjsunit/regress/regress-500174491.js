// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

const sr = new ShadowRealm();
const wrapped = sr.evaluate("(function f() { return 1; })");
d8.test.verifySourcePositions(wrapped);
