// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {v, w, x, y, z} from "modules-init3.js";

assertEquals({value: 40, done: true}, v().next());
assertSame(undefined, w);
assertThrows(() => x, ReferenceError);
assertThrows(() => y, ReferenceError);
assertThrows(() => z, ReferenceError);

export function check() {
  assertEquals({value: 40, done: true}, v().next());
  assertEquals(41, w);
  assertEquals(42, x);
  assertEquals("y", y.name);
  assertEquals("hello world", z);
  return true;
}
