// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-return-call --stack-size=64

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestFactorialReturnCall() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  const sig_i_iii = builder.addType(kSig_i_iii);

  // construct the code for the function
  // f_aux(N,X) where N=<1 => X
  // f_aux(N,X) => f_aux(N-1,X*N)
  let fact_aux = builder.addFunction("fact_aux",kSig_i_ii);
  fact_aux.addBody([
    kExprGetLocal, 0, kExprI32Const, 1, kExprI32LeS,
    kExprIf, kWasmI32,
      kExprGetLocal, 1,
    kExprElse,
      kExprGetLocal, 0,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprI32Mul,
      kExprReturnCall, fact_aux.index,
    kExprEnd
      ]);

  //main(N)=>fact_aux(N,1)
  let main = builder.addFunction("main", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprI32Const, 1,
          kExprReturnCall,0
    ]).exportFunc();

  let module = builder.instantiate();

  print(" --three--");
  assertEquals(6, module.exports.main(3));
  print(" --four--");
  assertEquals(24, module.exports.main(4));
})();

(function TestIndirectFactorialReturnCall() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  const sig_i_iii = builder.addType(kSig_i_iii);

  // construct the code for the function
  // fact(N) => f_ind(N,1,f).
  //
  // f_ind(N,X,_) where N=<1 => X
  // f_ind(N,X,F) => F(N-1,X*N,F).

  let f_ind = builder.addFunction("f_ind",kSig_i_iii).
      addBody([
    kExprGetLocal, 0, kExprI32Const, 1, kExprI32LeS,
    kExprIf, kWasmI32,
      kExprGetLocal, 1,
    kExprElse,
      kExprGetLocal, 0,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprI32Mul,
      kExprGetLocal, 2,
      kExprGetLocal, 2,
      kExprReturnCallIndirect, sig_i_iii, kTableZero,
    kExprEnd
      ]);

  //main(N)=>fact_aux(N,1)
  let main = builder.addFunction("main", kSig_i_i)
        .addBody([
      kExprGetLocal, 0,
      kExprI32Const, 1,
      kExprI32Const, f_ind.index,
      kExprReturnCall, f_ind.index
    ]).exportFunc();

  builder.appendToTable([f_ind.index, main.index]);

  let module = builder.instantiate();

  print(" --three--");
  assertEquals(6, module.exports.main(3));
  print(" --four--");
  assertEquals(24, module.exports.main(4));
})();

(function TestImportReturnCall() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  const sig_i_iii = builder.addType(kSig_i_iii);

  let pick = builder.addImport("q", "pick", sig_i_iii);

  let main = builder.addFunction("main", kSig_i_iii)
        .addBody([
          kExprGetLocal, 1,
          kExprGetLocal, 2,
          kExprGetLocal, 0,
          kExprReturnCall, pick
        ])
        .exportFunc();

  let module = builder.instantiate({q: {
    pick: function(a, b, c) { return c ? a : b; }}});

  print(" --left--");
  assertEquals(-2, module.exports.main(1, -2, 3));
  print(" --right--");
  assertEquals(3, module.exports.main(0, -2, 3));
})();

(function TestImportIndirectReturnCall() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  const sig_i_iii = builder.addType(kSig_i_iii);

  let pick = builder.addImport("q", "pick", sig_i_iii);
  const tableIndex = 3; // Arbitrary location of import

  builder.addElementSegment(0, tableIndex,false,[pick]);

  let main = builder.addFunction("main", kSig_i_iii)
        .addBody([
          kExprGetLocal, 1,
          kExprGetLocal, 2,
          kExprGetLocal, 0,
          kExprI32Const, tableIndex,
          kExprReturnCallIndirect, sig_i_iii, kTableZero
        ])
        .exportFunc();
  builder.appendToTable([pick, main.index]);

  let module = builder.instantiate({q: {
    pick: function(a, b, c) { return c ? a : b; }
  }});

  print(" --left--");
  assertEquals(-2, module.exports.main(1, -2, 3));
  print(" --right--");
  assertEquals(3, module.exports.main(0, -2, 3));
})();
