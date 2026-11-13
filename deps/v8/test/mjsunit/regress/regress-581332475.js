// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// On riscv64, MacroAssembler::Mulh32 multiplied the raw 64-bit registers
// without sign-extending its operands.  A Word32 with dirty upper bits, such
// as ~u32[0] (zero-extending lwu followed by a 64-bit xori), then escaped the
// range the typer had proven for constant signed % and /.

const u32 = new Uint32Array([0xfffffffe]);

// ~u32[0] is int32 +1, but the register holds 0xffffffff_00000001.
function mod5() { return (~u32[0] % 5) | 0; }
function div5() { return (~u32[0] / 5) | 0; }

assertEquals(1, ~u32[0]);
assertEquals(1, mod5());
assertEquals(0, div5());

%PrepareFunctionForOptimization(mod5);
%PrepareFunctionForOptimization(div5);
mod5();
div5();
%OptimizeFunctionOnNextCall(mod5);
%OptimizeFunctionOnNextCall(div5);

// Constant signed % and / are strength-reduced to Int32MulHigh.
assertEquals(1, mod5());
assertEquals(0, div5());
assertOptimized(mod5);
assertOptimized(div5);
