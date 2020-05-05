// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

const C = class {
  #a() {}
  test(obj) { obj.#a(); }
};
(new C).test({});
