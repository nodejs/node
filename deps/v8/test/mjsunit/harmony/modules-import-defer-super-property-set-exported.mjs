// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-1.mjs';

assertEquals(0, globalThis.eval_list.length);

class A { constructor() { return ns; } }
class B extends A {
  constructor() {
    super();
    super.foo = 14;
  }
}

try { new B(); } catch (_) {}

// super.foo = 14 calls OrdinarySetWithOwnDescriptor which calls
// Receiver.[[GetOwnProperty]]("foo") on the namespace, triggering evaluation.
assertArrayEquals(['defer-1'], globalThis.eval_list);
