// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

class C {
  m() {
    return super.stack;
  }
  mKeyed(k) {
    return super[k];
  }
}
Object.setPrototypeOf(C.prototype, Error());

// Api getters without signature checks (like Error.prototype.stack) convert
// undefined/null receiver to global_proxy (returning undefined) without
// throwing.
assertEquals(undefined, new C().m.call(undefined));
assertEquals(undefined, new C().mKeyed.call(undefined, 'stack'));
