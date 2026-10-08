// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

const MAX_PARAMS = 6500;
const args = new Array(MAX_PARAMS).fill('a').map((v, i) => v + i).join(',');
const gen = eval(`(function* (${args}) { yield 42; })`);

const it = gen();

function triggerNearLimit() {
  try {
    triggerNearLimit();
  } catch (e) {
    it.next();
  }
}

triggerNearLimit();
