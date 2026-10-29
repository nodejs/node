// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var life;
var msg;
import('modules-skip-1.mjs').then(namespace => life = namespace.life());
import('modules-skip-2.mjs').catch(err => msg = err.message);

assertEquals(undefined, life);
assertEquals(undefined, msg);

%PerformMicrotaskCheckpoint();

assertEquals(42, life);
assertEquals('42 is not the answer', msg);
