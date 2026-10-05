// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --allow-natives-syntax

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-1.mjs';

assertEquals(0, globalThis.eval_list.length);

// Collecting private names is what the inspector does when it computes the
// internal properties of an object. It must not trigger the evaluation of the
// deferred module, since the inspector runs it while JavaScript execution is
// disallowed.
assertThrows(() => %GetPrivateMember(ns, 'field'), Error);

assertEquals(0, globalThis.eval_list.length);

// Regular key collection still triggers the evaluation.
assertArrayEquals(['foo'], Object.keys(ns));
assertArrayEquals(['defer-1'], globalThis.eval_list);
