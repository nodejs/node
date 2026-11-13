// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

let s = '\u0800'.repeat(16);
for (let i = 0; i < 27; i++) {
  s = s + s;
  Sandbox.corruptObjectField(s, 'length', 32);
}
Sandbox.corruptObjectField(s, 'length', 0x55555555);

if (typeof Intl !== 'undefined') {
  new Intl.DisplayNames('en', {type: 'language'}).of(s);
} else {
  %StringToCString(s);
}
assertUnreachable('Corrupted String length should be caught by SBXCHECK');
