// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testScopeResolutionInPatternDefault() {
  let [c = class A { field = x; }, x] = [undefined, 42];
  assertEquals(42, new c().field);
}
testScopeResolutionInPatternDefault();
