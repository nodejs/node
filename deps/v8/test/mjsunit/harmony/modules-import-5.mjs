// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import

var life;
let x = 'modules-skip-1.mjs';
import(x).then(namespace => life = namespace.life());
x = 'modules-skip-2.mjs';

%PerformMicrotaskCheckpoint();
assertEquals(42, life);
