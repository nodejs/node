// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Required to make a spidermonkey dependency more permissive:
// spidermonkey/test/expected/import/files/local/smTempBranch/shell.js
var document = {all: undefined};

try {
  drainJobQueue;
} catch(e) {
  this.drainJobQueue = this.nop;
}
