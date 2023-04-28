// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const array = new Uint8Array(1024);
Uint8Array.__proto__ = {
  __proto__: Uint16Array.__proto__,
  [Symbol.species]: Uint16Array,
};

const uint16 = array.slice();
assertTrue(uint16 instanceof Uint16Array);
assertEquals(uint16.length, 1024);
