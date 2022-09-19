// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-turbo-reduction

function foo() {
  const v11 = new Int8Array(150);
  Object(v11,...v11,v11);
}

for (i = 0; i < 100; i++)
  foo();
