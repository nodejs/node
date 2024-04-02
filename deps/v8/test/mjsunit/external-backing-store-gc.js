// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator --mock-arraybuffer-allocator-limit=1300000000

// --mock-arraybuffer-allocator-limit should be above the hard limit external
// for memory. Below that limit anything is opportunistic and may be delayed,
// e.g., by tasks getting stalled and the event loop not being invoked.

for (var i = 0; i < 1536; i++) {
  let garbage = new ArrayBuffer(1024*1024);
}
