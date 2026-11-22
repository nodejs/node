// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --allow-natives-syntax

// JS_BUNDLE_MODULE:mBad.mjs
syntax error!

// JS_BUNDLE_MODULE:mB.mjs
import './mA.mjs';

// JS_BUNDLE_MODULE:mA.mjs
import './mB.mjs';
import './mBad.mjs';

// JS_BUNDLE_SCRIPT
let caughtA = false;
let caughtB = false;

import('mA.mjs').catch(e => {
  caughtA = true;
});
%PerformMicrotaskCheckpoint();
assertTrue(caughtA);

import('mB.mjs').catch(e => {
  caughtB = true;
});
%PerformMicrotaskCheckpoint();
assertTrue(caughtB);
