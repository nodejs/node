// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-1.mjs';

assertEquals(0, globalThis.eval_list.length);

const obj = Object.create(ns);

assertEquals(undefined, Object.prototype.__lookupSetter__.call(obj, 'foo'));
assertArrayEquals(['defer-1'], globalThis.eval_list);

assertEquals(undefined, Object.prototype.__lookupSetter__.call(obj, 'non-existent'));
assertArrayEquals(['defer-1'], globalThis.eval_list);
