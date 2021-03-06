// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import

var error1, error2;
import('modules-skip-11.mjs').catch(e => error1 = e);
import('modules-skip-11.mjs').catch(e => error2 = e);
%PerformMicrotaskCheckpoint();

assertEquals(error1, error2);
assertEquals(typeof error1, "symbol");
