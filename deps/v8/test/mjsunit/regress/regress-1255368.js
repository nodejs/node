// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-regexp-tier-up --no-enable-experimental-regexp-engine
// Flags: --no-regexp-interpret-all

const arr = new Array(20000).fill([1]);
const regexp = RegExp(JSON.stringify(arr));
assertThrows(() => regexp.exec(), SyntaxError, /Regular expression too large/);
