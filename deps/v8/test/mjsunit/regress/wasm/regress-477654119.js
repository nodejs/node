// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

// Tests that we do not access past the end of the function if we get an
// instruction frequency hint with an offset greater than the function length.
// We cannot use WasmModuleBuilder here because the code section has to be last.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const bin = new Binary();
bin.emit_header();

// Type section: (func (result i32))
bin.emit_section(kTypeSectionCode, section => {
  section.emit_u32v(1);
  section.emit_u8(kWasmFunctionTypeForm);
  section.emit_u32v(0); // params
  section.emit_u32v(1); // results
  section.emit_u8(kWasmI32);
});

// Function section: one function of type 0.
bin.emit_section(kFunctionSectionCode, section => {
  section.emit_u32v(1);
  section.emit_u32v(0);
});

// Export section: export func 0 as "f".
bin.emit_section(kExportSectionCode, section => {
  section.emit_u32v(1);
  section.emit_string('f');
  section.emit_u8(kExternalFunction);
  section.emit_u32v(0);
});

// Custom section: metadata.code.instr_freq
const instr = new Binary();
instr.emit_u32v(1);    // number of functions
instr.emit_u32v(0);    // function index
instr.emit_u32v(1);    // hints count
instr.emit_u32v(0x100);  // byte offset (OOB)
instr.emit_u32v(1);    // hint length
instr.emit_u8(1);      // frequency

bin.emit_section(kUnknownSectionCode, section => {
  section.emit_string('metadata.code.instr_freq');
  section.emit_bytes(instr.trunc_buffer());
});

// Code section: one body.
bin.emit_section(kCodeSectionCode, section => {
  section.emit_u32v(1);
  const body = new Binary();
  body.emit_u32v(0); // locals
  body.emit_u8(kExprI32Const);
  body.emit_u32v(0);
  body.emit_u8(kExprEnd);
  section.emit_u32v(body.length);
  section.emit_bytes(body.trunc_buffer());
});

const module = new WebAssembly.Module(bin.trunc_buffer());
const instance = new WebAssembly.Instance(module);
instance.exports.f();
