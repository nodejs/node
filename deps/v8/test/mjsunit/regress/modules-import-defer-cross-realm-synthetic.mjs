// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-import-text --js-defer-import-eval

let r1 = Realm.create();

import defer * as ns from 'data:text/javascript,This is text content!' with { type: 'text' };
Realm.shared = ns;

Realm.eval(1, "let ns = Realm.shared; let val = ns.default; if (val !== 'This is text content!') throw new Error('Assertion failed');");
