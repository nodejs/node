// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof Function('return this;')().test262 === 'undefined') {
  Function('return this;')().test262 = ['1_udir_a'];
} else {
  Function('return this;')().test262.push('1_udir_a');
}
if (false) {
  assertUnreachable();
  await 42;
}
Function('return this;')().test262.push('1_udir_b');
