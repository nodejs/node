// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C {
  set #a(val) {}
  constructor() {
    const a = this.#a;
  }
}
new C;
