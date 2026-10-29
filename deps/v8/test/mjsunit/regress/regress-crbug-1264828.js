// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

{
  class C {
    x = Object.freeze(this);
  }
  // Call once to install slow handler.
  assertThrows(() => { new C(); });
  // Hit the slow handler.
  assertThrows(() => { new C(); });
}
