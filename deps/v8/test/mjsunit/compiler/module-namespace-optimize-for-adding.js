// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --allow-natives-syntax

// JS_BUNDLE_MODULE:mod.mjs
export let exported = 1;

// JS_BUNDLE_MODULE_ENTRYPOINT
import * as ns from './mod.mjs';

const o = Object.create(ns);
function loadProto(x) {
  return x.exported;
}
%PrepareFunctionForOptimization(loadProto);
assertEquals(1, loadProto(o));
assertEquals(1, loadProto(o));
assertTrue(%HasFastProperties(ns));

function loadNs(x) {
  return x.exported;
}
%PrepareFunctionForOptimization(loadNs);
assertEquals(1, loadNs(ns));
assertEquals(1, loadNs(ns));
%OptimizeObjectForAddingMultipleProperties(ns, 1);
assertTrue(%HasFastProperties(ns));
%OptimizeFunctionOnNextCall(loadNs);
assertEquals(1, loadNs(ns));
