// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const workerScript = `
  postMessage("started");
  const obj = { toString: () => "any_file_name" };
  while (true) {
    d8.file.exists(obj);
  }
`;

for (let i = 0; i < 100; i++) {
  const w = new Worker(workerScript, {type: 'string'});
  assertEquals("started", w.getMessage());
  w.terminate();
}
