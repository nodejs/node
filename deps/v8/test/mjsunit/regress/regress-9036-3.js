// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function C() {};

const p = new Proxy({}, { getPrototypeOf() { return C.prototype } });
const o = Object.create(p);

assertTrue(o instanceof C);
assertTrue(o instanceof C);
