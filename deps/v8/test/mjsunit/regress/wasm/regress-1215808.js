// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const k4GB = 4 * 1024 * 1024 * 1024;

let memory = new WebAssembly.Memory({initial: 1});
try {
  memory.grow(k4GB - 1);
} catch {}
