// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-externalize-string --cache=after-execute

let s = createExternalizableString("A".repeat(32));
externalizeString(s, true);
let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let s_addr = Sandbox.getAddressOf(s);

function target() { return 1; }
target();

let sfi = memory.getUint32(Sandbox.getAddressOf(target) + 16, true) - 1;
let script = memory.getUint32(sfi + 20, true) - 1;
// Attach external string s to script.source_mapping_url_ (offset 64)
memory.setUint32(script + 64, s_addr + 1, true);

// Corrupt length of ExternalString s from 32 to 100000:
memory.setInt32(s_addr + 8, 100000, true);
