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
    super.notExported = 14;
  }
}

try { new B(); } catch (_) {}

// Even for non-exported keys, [[GetOwnProperty]] calls GetModuleExportsList
// which triggers evaluation.
assertArrayEquals(['defer-1'], globalThis.eval_list);
