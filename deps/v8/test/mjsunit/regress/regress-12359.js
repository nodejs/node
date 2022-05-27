// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan --assert-types --no-analyze-environment-liveness

class C {
    get #a() { }
    getA() { return this.#a; }
}

new C().getA();
