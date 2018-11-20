// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

PAGES = 10;
memory = new WebAssembly.Memory({initial: PAGES});
buffer = memory.buffer;
buffer = new Uint8Array(buffer);
memory.grow();
WebAssembly.validate(buffer);
