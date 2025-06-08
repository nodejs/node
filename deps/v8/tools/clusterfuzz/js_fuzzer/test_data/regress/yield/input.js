// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function* it() {
  let foo = yield 1;
  let bar = 3 + (yield 2);
}
