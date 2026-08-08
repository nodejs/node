// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

function getCallSiteInfo(site) {
  const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const site_addr = Sandbox.getAddressOf(site);
  const properties = mem.getUint32(site_addr + 4, true) - 1;
  return mem.getUint32(properties + 8, true) - 1;
}

function AsmModule(stdlib, foreign, heap) {
  "use asm";
  var imp = foreign.imp;
  function f() {
    imp();
  }
  return { f: f };
}

let captured_sites;
Error.prepareStackTrace = function(error, sites) {
  captured_sites = sites;
  return "stack";
};

const imported = function() {
  new Error().stack;
};

const instance = AsmModule(this, { imp: imported }, new ArrayBuffer(65536));
instance.f();

const asm_site = captured_sites[1];
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const call_site_info = getCallSiteInfo(asm_site);

// Corrupt the stack-captured wasm byte offset so asm.js source-position lookup
// lower_bounds past the offset vector.
mem.setUint32(call_site_info + 16, 0x7ffffffe, true);

asm_site.getLineNumber();
