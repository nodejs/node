// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-no-exports.mjs';

assertEquals(0, globalThis.eval_list.length);

// A side-effect-only module has no export to read, so [[OwnPropertyKeys]] is
// the a way to force its evaluation.
assertArrayEquals([], Object.keys(ns));

assertArrayEquals(['defer-no-exports'], globalThis.eval_list);
