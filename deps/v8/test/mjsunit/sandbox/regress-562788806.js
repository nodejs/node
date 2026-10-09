// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --cache=after-execute --always-sparkplug --sparkplug

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const sfi = Sandbox.dereferenceTaggedPointerField(
    function() {}, 'shared_function_info');
const script = Sandbox.dereferenceTaggedPointerField(sfi, 'script');
const scriptAddr = Sandbox.getAddressOf(script);

const params = Array.from({length: 3328}, (_, i) => 'p' + i).join(',');
(0, eval)(
    `function foo(${params}) { (() => new.target)(); p0.a; p0.b; v0 = 0; ` +
    `return 65535; var [v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13] = []; }`);
foo({a: 1, b: 2});

const fooAddr = Sandbox.getAddressOf(foo);
const kWrappedArgumentsOffset = 36;
const kFeedbackCellOffset = 24;
const kDispatchHandleOffset = 12;

const existing = mem.getUint32(scriptAddr + kWrappedArgumentsOffset, true);
if (existing !== 0x11) {
  const cellAddr = (existing & ~3) >>> 0;
  const corruptedHandle = mem.getUint32(cellAddr + 8, true);
  mem.setUint32(fooAddr + kDispatchHandleOffset, corruptedHandle, true);
  foo();
} else {
  const fbCellAddr = mem.getUint32(fooAddr + kFeedbackCellOffset, true);
  mem.setUint32((fbCellAddr & ~3) + 4, 0x11, true);
  mem.setUint32(scriptAddr + kWrappedArgumentsOffset, fbCellAddr, true);
}
