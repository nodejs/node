// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-code-coverage --wasm-staging

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function DoCoverageResultsMatchExpectations(result, expectation) {
  if (result.length != expectation.length) return false;

  for (let i = 0; i < result.length; i++) {
    if (result[i].start != expectation[i].start ||
      result[i].end != expectation[i].end ||
      result[i].count != expectation[i].count) {
      return false;
    }
  }

  return true;
}

function GetCoverage() {
  let wasmScriptsCoverage = %DebugCollectWasmCoverage();
  // Assume there is a single Wasm module per test.
  return wasmScriptsCoverage[wasmScriptsCoverage.length - 1];
}

function TestCoverage(name, source, expectation) {
  eval('(' + source + ')();');
  var covData = GetCoverage();
  const mismatch = !DoCoverageResultsMatchExpectations(covData, expectation);
  if (mismatch) {
    console.log("=== Coverage Expectation ===")
    for (const { start, end, count } of expectation) {
      console.log(`Range [${start}, ${end}] (count: ${count})`);
    }
    console.log("=== Coverage Results ===")
    for (const { start, end, count } of covData) {
      console.log(`Range [${start}, ${end}] (count: ${count})`);
    }
    console.log("========================")
  }
  assertTrue(!mismatch, name + " failed");
};

function testSimpleWasmCode() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
//-----------------------------
      ...wasmI32Const(42), // 1
      ...wasmI32Const(11), // 3
      kExprI32Add,         // 5
    ])                     // 6
//-----------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  let result = instance.exports.main();
  assertEquals(53, result);
}
TestCoverage(
  "test coverage for simple wasm code", testSimpleWasmCode.toString(),
  [{ "start": 0, "end": 6, "count": 1 }]
);

function testIgnoreUnreachableCode() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
//-----------------------------
      kExprUnreachable,    // 1
//-----------------------------
      ...wasmI32Const(42), // 2
      ...wasmI32Const(11), // 4
      kExprI32Add,         // 6
    ])                     // 7
//-----------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertTraps(kTrapUnreachable, () => instance.exports.main());
}
TestCoverage(
  "test coverage ignores unreachable code",
  testIgnoreUnreachableCode.toString(),
  [{ "start": 0, "end": 1, "count": 1 }, { "start": 2, "end": 7, "count": 0 }]
);

function testCoverageWithBrIfAndReturn() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------
      kExprBlock, kWasmVoid,    //  1
        kExprLocalGet, 0,       //  3
        kExprBrIf, 0,           //  5
//-----------------------------------
        ...wasmI32Const(1),     //  7
        kExprReturn,            //  9
//-----------------------------------
      kExprEnd,                 // 10
//-----------------------------------
      ...wasmI32Const(0),       // 11
    ])                          // 13
//-----------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(0));
  assertEquals(0, instance.exports.main(2));
}
TestCoverage(
  "test coverage with br_if and return",
  testCoverageWithBrIfAndReturn.toString(),
  [{ "start": 0, "end": 6, "count": 2 },
  { "start": 7, "end": 9, "count": 1 },
  { "start": 10, "end": 10, "count": 0 },
  { "start": 11, "end": 13, "count": 1 }]
);

function testCoverageWithIfEnd() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------
      kExprLocalGet, 0,         //  1
      kExprIf, kWasmVoid,       //  3
//-----------------------------------
      ...wasmI32Const(0),       //  5
      kExprReturn,              //  7
//-----------------------------------
      kExprEnd,                 //  8
//-----------------------------------
      ...wasmI32Const(1),       //  9
    ])                          // 11
//-----------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(0));
  assertEquals(0, instance.exports.main(2));
}
TestCoverage(
  "test coverage with if/end", testCoverageWithIfEnd.toString(),
  [{ "start": 0, "end": 4, "count": 2 },
  { "start": 5, "end": 7, "count": 1 },
  { "start": 8, "end": 8, "count": 0 },
  { "start": 9, "end": 11, "count": 1 }]
);

function testCoverageWithIfElseEnd() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------
      kExprLocalGet, 0,         //  1
      kExprIf, kWasmVoid,       //  3
//-----------------------------------
      ...wasmI32Const(0),       //  5
      kExprReturn,              //  7
//-----------------------------------
      kExprElse,                //  8
//-----------------------------------
      ...wasmI32Const(1),       //  9
      kExprReturn,              // 11
//-----------------------------------
      kExprEnd,                 // 12
//-----------------------------------
      ...wasmI32Const(-1),      // 13
    ])                          // 15
//-----------------------------------
   .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(0));
  assertEquals(0, instance.exports.main(2));
}
TestCoverage(
  "test coverage with if/else/end", testCoverageWithIfElseEnd.toString(),
  [{ "start": 0, "end": 4, "count": 2 },
  { "start": 5, "end": 7, "count": 1 },
  { "start": 8, "end": 8, "count": 0 },
  { "start": 9, "end": 11, "count": 1 },
  { "start": 12, "end": 12, "count": 0 },
  { "start": 13, "end": 15, "count": 0 }]
);

function testCoverageWithIfElseEndWithBranch() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------
      kExprLocalGet, 0,         //  1
      kExprIf, kWasmVoid,       //  3
//-----------------------------------
      kExprBr, 0,               //  5
//-----------------------------------
      kExprElse,                //  7
//-----------------------------------
      ...wasmI32Const(1),       //  8
      kExprReturn,              // 10
//-----------------------------------
      kExprEnd,                 // 11
//-----------------------------------
      ...wasmI32Const(0),       // 12
    ])                          // 14
//-----------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(0));
  assertEquals(0, instance.exports.main(2));
}
TestCoverage(
  "test coverage with if/else/end with branch",
  testCoverageWithIfElseEndWithBranch.toString(),
  [{ "start": 0, "end": 4, "count": 2 },
  { "start": 5, "end": 6, "count": 1 },
  { "start": 7, "end": 7, "count": 0 },
  { "start": 8, "end": 10, "count": 1 },
  { "start": 11, "end": 11, "count": 0 },
  { "start": 12, "end": 14, "count": 1 }]
);

function testCoverageWithIfElseEndWithoutBranches() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------
      kExprLocalGet, 0,         //  1
      kExprIf, kWasmI32,        //  3
//-----------------------------------
      ...wasmI32Const(0),       //  5
      kExprElse,                //  7
//-----------------------------------
      ...wasmI32Const(1),       //  8
      kExprEnd,                 // 10
    ])                          // 11
//-----------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(0));
  assertEquals(0, instance.exports.main(2));
}
TestCoverage(
  "test coverage with if/else/end without branches",
  testCoverageWithIfElseEndWithoutBranches.toString(),
  [{ "start": 0, "end": 4, "count": 2 },
  { "start": 5, "end": 7, "count": 1 },
  { "start": 8, "end": 10, "count": 1 },
  { "start": 11, "end": 11, "count": 2 }]
);

function testCoverageWithBrTable() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  var kSig_d_idd = makeSig([kWasmI32, kWasmF64, kWasmF64], [kWasmF64]);

  builder.addFunction("calc", kSig_d_idd)
    .addBody([
//---------------------------------------------------
      kExprBlock, kWasmVoid,                    //  1
        kExprBlock, kWasmVoid,                  //  3
          kExprBlock, kWasmVoid,                //  5
            kExprBlock, kWasmVoid,              //  7
              kExprBlock, kWasmVoid,            //  9
                kExprLocalGet, 0,               // 11
                kExprBrTable, 4, 0, 1, 2, 3, 4, // 13
//---------------------------------------------------
              kExprEnd,                         // 20
//---------------------------------------------------
              kExprLocalGet, 1,                 // 21
              kExprLocalGet, 2,                 // 23
              kExprF64Add,                      // 25
              kExprReturn,                      // 26
//---------------------------------------------------
            kExprEnd,                           // 27
//---------------------------------------------------
            kExprLocalGet, 1,                   // 28
            kExprLocalGet, 2,                   // 30
            kExprF64Sub,                        // 32
            kExprReturn,                        // 33
//---------------------------------------------------
          kExprEnd,                             // 34
//---------------------------------------------------
          kExprLocalGet, 1,                     // 35
          kExprLocalGet, 2,                     // 37
          kExprF64Mul,                          // 39
          kExprReturn,                          // 40
//---------------------------------------------------
        kExprEnd,                               // 41
//---------------------------------------------------
        kExprLocalGet, 1,                       // 42
        kExprLocalGet, 2,                       // 44
        kExprF64Div,                            // 46
        kExprReturn,                            // 47
//---------------------------------------------------
      kExprEnd,                                 // 48
//---------------------------------------------------
      ...wasmF64Const(.0),                      // 49
    ])                                          // 58
//---------------------------------------------------
    .exportAs("calc");

  instance = builder.instantiate({});
  const kAdd = 0;
  const kSub = 1;
  const kMul = 2;
  const kDiv = 3;
  assertEquals(5.0, instance.exports.calc(kAdd, 3.0, 2.0));
  assertEquals(1.0, instance.exports.calc(kSub, 3.0, 2.0));
  assertEquals(6.0, instance.exports.calc(kMul, 3.0, 2.0));
  assertEquals(1.5, instance.exports.calc(kDiv, 3.0, 2.0));
  assertEquals(0.0, instance.exports.calc(4, 3.0, 2.0));
}
TestCoverage(
  "test coverage with br_table",
  testCoverageWithBrTable.toString(),
  [{ "start": 0, "end": 19, "count": 5 },
  { "start": 20, "end": 20, "count": 0 },
  { "start": 21, "end": 26, "count": 1 },
  { "start": 27, "end": 27, "count": 0 },
  { "start": 28, "end": 33, "count": 1 },
  { "start": 34, "end": 34, "count": 0 },
  { "start": 35, "end": 40, "count": 1 },
  { "start": 41, "end": 41, "count": 0 },
  { "start": 42, "end": 47, "count": 1 },
  { "start": 48, "end": 48, "count": 0 },
  { "start": 49, "end": 58, "count": 1 }]
);

function testCoverageWithLoop() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("factorial", kSig_i_i)
    .addLocals(kWasmI32, 1)
    .addBody([
      ...wasmI32Const(1),         //  3
      kExprLocalSet, 1,           //  5
      kExprBlock, kWasmVoid,      //  7
//------------------------------------- * 12
        kExprLoop, kWasmVoid,     //  9
          kExprLocalGet, 0,       // 11
          ...wasmI32Const(0),     // 13
          kExprI32Eq,             // 15
          kExprBrIf, 1,           // 16
//------------------------------------- * 11
          kExprLocalGet, 1,       // 18
          kExprLocalGet, 0,       // 20
          kExprI32Mul,            // 22
          kExprLocalSet, 1,       // 23
          kExprLocalGet, 0,       // 25
          ...wasmI32Const(1),     // 27
          kExprI32Sub,            // 29
          kExprLocalSet, 0,       // 30
          kExprBr, 0,             // 32
//-------------------------------------
       kExprEnd,                  // 34
//-------------------------------------
     kExprEnd,                    // 35
//-------------------------------------
     kExprLocalGet, 1,            // 36
    ])                            // 38
    .exportAs("factorial");

  instance = builder.instantiate({});
  assertEquals(39916800, instance.exports.factorial(11));
}
TestCoverage(
  "test coverage with loop", testCoverageWithLoop.toString(),
  [{ "start": 0, "end": 8, "count": 1 },
  { "start": 9, "end": 17, "count": 12 },
  { "start": 18, "end": 33, "count": 11 },
  { "start": 34, "end": 34, "count": 0 },
  { "start": 35, "end": 35, "count": 0 },
  { "start": 36, "end": 38, "count": 1 }]
);

function testCoverageWithBr() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
//------------------------------------
      kExprBlock, kWasmVoid,     //  1
        kExprBlock, kWasmVoid,   //  3
          kExprBr, 0,            //  5
//------------------------------------
          ...wasmI32Const(0),    //  7
          kExprReturn,           //  9
//------------------------------------
        kExprEnd,                // 10
//------------------------------------
        ...wasmI32Const(1),      // 11
        kExprReturn,             // 13
//------------------------------------
      kExprEnd,                  // 14
//------------------------------------
      ...wasmI32Const(-1),       // 15
    ])                           // 17
//------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(1, instance.exports.main());
}
TestCoverage(
  "test coverage with br", testCoverageWithBr.toString(),
  [{ "start": 0, "end": 6, "count": 1 },
  { "start": 7, "end": 9, "count": 0 },
  { "start": 10, "end": 10, "count": 0 },
  { "start": 11, "end": 13, "count": 1 },
  { "start": 14, "end": 14, "count": 0 },
  { "start": 15, "end": 17, "count": 0 }]
);

function testCoverageWithRefBranch() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let array_type_index = builder.addArray(kWasmI32, true);

  builder.addFunction("main", kSig_i_v)
    .addBody([
//-------------------------------------------
      kExprBlock, kWasmVoid,            //  1
        kExprRefNull, array_type_index, //  3
        kExprBrOnNull, 0,               //  5
//-------------------------------------------
        ...wasmI32Const(1),             //  7
        kExprReturn,                    //  9
//-------------------------------------------
      kExprEnd,                         // 10
//-------------------------------------------
      ...wasmI32Const(0),               // 11
    ])                                  // 13
//-------------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(0, instance.exports.main());
}
TestCoverage(
  "test coverage with Ref branch", testCoverageWithRefBranch.toString(),
  [{ "start": 0, "end": 6, "count": 1 },
  { "start": 7, "end": 9, "count": 0 },
  { "start": 10, "end": 10, "count": 0 },
  { "start": 11, "end": 13, "count": 1 }]
);

function testCoverageWithNestedLegacyTryCatch() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_i_v)
   .addLocals(kWasmI32, 1)
   .addBody([
//-----------------------------------------
      kExprTry, kWasmI32,             //  3
        ...wasmF32Const(3.14),        //  5
        ...wasmF32Const(3.14),        // 10
        kExprTry, kWasmI32,           // 15
          ...wasmI32Const(1),         // 17
          kExprThrow, except,         // 19
//-----------------------------------------
        kExprCatch, except,           // 21
//-----------------------------------------
          kExprTry, kWasmI32,         // 23
            ...wasmI32Const(2),       // 25
          kExprEnd,                   // 27
//-----------------------------------------
        kExprCatchAll,                // 28
//-----------------------------------------
          ...wasmI32Const(3),         // 29
        kExprEnd,                     // 31
//-----------------------------------------
        kExprLocalSet, 0,             // 32
        kExprF32Ne,                   // 34
        kExprDrop,                    // 35
        kExprLocalGet, 0,             // 36
      kExprEnd,                       // 38
//-----------------------------------------
    ])                                // 39
//-----------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(2, instance.exports.main());
}
TestCoverage(
  "test coverage with nested legacy try/catch",
  testCoverageWithNestedLegacyTryCatch.toString(),
  [{ "start": 0, "end": 20, "count": 1 },
  { "start": 21, "end": 22, "count": 0 }, // Is this correct?
  { "start": 23, "end": 27, "count": 1 },
  { "start": 28, "end": 28, "count": 1 }, // Is this correct?
  { "start": 29, "end": 31, "count": 0 },
  { "start": 32, "end": 38, "count": 1 },
  { "start": 39, "end": 39, "count": 1 }]
);

function testCoverageWithNestedLegacyTryCatchWithNoThrow() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_i_v)
    .addBody([
//-----------------------------------------
      kExprTry, kWasmI32,             //  1
        ...wasmF32Const(3.14),        //  3
        ...wasmF32Const(3.14),        //  8
        kExprTry, kWasmI32,           // 13
          ...wasmI32Const(1),         // 15
        kExprCatch, except,           // 17
//-----------------------------------------
          kExprTry, kWasmI32,         // 19
            ...wasmI32Const(2),       // 21
          kExprEnd,                   // 23
//-----------------------------------------
        kExprCatchAll,                // 24
//-----------------------------------------
          ...wasmI32Const(3),         // 25
        kExprEnd,                     // 27
//-----------------------------------------
        kExprDrop,                    // 28
        kExprF32Ne,                   // 29
      kExprEnd,                       // 30
//-----------------------------------------
    ])                                // 31
//-----------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(0, instance.exports.main());
}
TestCoverage(
  "test coverage with nested legacy try/catch with no throw",
  testCoverageWithNestedLegacyTryCatchWithNoThrow.toString(),
  [{ "start": 0, "end": 18, "count": 1 },
  { "start": 19, "end": 23, "count": 0 },
  { "start": 24, "end": 24, "count": 0 },
  { "start": 25, "end": 27, "count": 0 },
  { "start": 28, "end": 30, "count": 1 },
  { "start": 31, "end": 31, "count": 1 }]
);

function testCoverageWithNewEHWithNoThrow() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------------
        kExprBlock, kWasmVoid,        //  1
          kExprTryTable, kWasmI32, 1, //  3
          kCatchNoRef, except, 0,
            kExprLocalGet, 0,         //  9
            kExprI32Eqz,              // 11
            kExprIf, kWasmVoid,       // 12
//-----------------------------------------
              kExprI32Const, 23,      // 14
              kExprBr, 1,             // 16
//-----------------------------------------
            kExprEnd,                 // 18
//-----------------------------------------
            kExprI32Const, 42,        // 19
          kExprEnd,                   // 21
//-----------------------------------------
          kExprReturn,                // 22
//-----------------------------------------
        kExprEnd,                     // 23
//-----------------------------------------
        kExprI32Const, 23             // 24
    ])                                // 26
//-----------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(23, instance.exports.main(0));
  assertEquals(42, instance.exports.main(1));
}
TestCoverage(
  "test coverage with new exception handling with no throw",
  testCoverageWithNewEHWithNoThrow.toString(),
  [{ "start": 0, "end": 13, "count": 2 },
  { "start": 14, "end": 17, "count": 1 },
  { "start": 18, "end": 18, "count": 0 },
  { "start": 19, "end": 21, "count": 1 },
  { "start": 22, "end": 22, "count": 2 },
  { "start": 23, "end": 23, "count": 0 },
  { "start": 24, "end": 26, "count": 0 }]
);

function testCoverageWithNewEH() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_i_i)
    .addBody([
//-----------------------------------------
        kExprBlock, kWasmVoid,        //  1
          kExprTryTable, kWasmI32, 1, //  3
          kCatchNoRef, except, 0,
            kExprLocalGet, 0,         //  9
            kExprI32Eqz,              // 11
            kExprIf, kWasmVoid,       // 12
//-----------------------------------------
              kExprThrow, except,     // 14
//-----------------------------------------
            kExprEnd,                 // 16
//-----------------------------------------
            kExprI32Const, 42,        // 17
          kExprEnd,                   // 19
//-----------------------------------------
          kExprBr, 1,                 // 20
//-----------------------------------------
        kExprEnd,                     // 22
//-----------------------------------------
        kExprI32Const, 23             // 23
    ])                                // 24
//-----------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  assertEquals(23, instance.exports.main(0));
  assertEquals(42, instance.exports.main(1));
  return instance.exports.main;
}
TestCoverage(
  "test coverage with new exception handling",
  testCoverageWithNewEH.toString(),
  [{ "start": 0, "end": 13, "count": 2 },
  { "start": 14, "end": 15, "count": 1 },
  { "start": 16, "end": 16, "count": 0 },
  { "start": 17, "end": 19, "count": 1 },
  { "start": 20, "end": 21, "count": 1 },
  { "start": 22, "end": 22, "count": 0 },
  { "start": 23, "end": 25, "count": 1 }]
);

function testCoverageWithComplexCode() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_v_i)
    .addLocals(kWasmI32, 1)
    .addBody([
//--------------------------------------------count-
      kExprLocalGet, 0,               //  3     2
      kExprIf, kWasmVoid,             //  5
//--------------------------------------------------
        kExprI32Const, 5,             //  7     1
        kExprLocalSet, 1,             //  9
        kExprLocalGet, 0,             // 11
        kExprIf, kWasmVoid,           // 13
//--------------------------------------------------
          kExprLoop, kWasmVoid,       // 15     5
            kExprBlock, 0,            // 17
              kExprLocalGet, 0,       // 19
              kExprIf, kWasmVoid,     // 21
//--------------------------------------------------
                ...wasmI32Const(1),   // 23     5
                kExprDrop,            // 25
                kExprBr, 1,           // 26
//--------------------------------------------------
              kExprEnd,               // 28
//--------------------------------------------------
              kExprLocalGet, 1,       // 29     0
              kExprBrIf, 0,           // 31
//--------------------------------------------------
              ...wasmI32Const(1),     // 33     0
              kExprDrop,              // 35
            kExprEnd,                 // 36
//--------------------------------------------------
            kExprLocalGet, 1,         // 37     5
            ...wasmI32Const(1),       // 39
            kExprI32Sub,              // 41
            kExprLocalTee, 1,         // 42
            ...wasmI32Const(0),       // 44
            kExprI32Ne,               // 46
            kExprBrIf, 0,             // 47
//--------------------------------------------------
            ...wasmI32Const(1),       // 49     1
            kExprDrop,                // 51
          kExprEnd,                   // 52
//--------------------------------------------------
          ...wasmI32Const(1),         // 53     1
          kExprDrop,                  // 55
        kExprEnd,                     // 56
//--------------------------------------------------
        ...wasmI32Const(1),           // 57     1
        kExprDrop,                    // 59
      kExprElse,                      // 60     1
//--------------------------------------------------
        ...wasmI32Const(1),           // 61
        ...wasmI32Const(2),           // 63
        kExprI32Add,                  // 65
        kExprDrop,                    // 66
      kExprEnd,                       // 67
//--------------------------------------------------
    ])                                // 68     2
//--------------------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  instance.exports.main(1);
  instance.exports.main(0);
}
TestCoverage(
  "test coverage with complex code", testCoverageWithComplexCode.toString(),
  [{ "start": 0, "end": 6, "count": 2 },
  { "start": 7, "end": 14, "count": 1 },
  { "start": 15, "end": 22, "count": 5 },
  { "start": 23, "end": 27, "count": 5 },
  { "start": 28, "end": 28, "count": 0 },
  { "start": 29, "end": 32, "count": 0 },
  { "start": 33, "end": 36, "count": 0 },
  { "start": 37, "end": 48, "count": 5 },
  { "start": 49, "end": 52, "count": 1 },
  { "start": 53, "end": 56, "count": 1 },
  { "start": 57, "end": 60, "count": 1 },
  { "start": 61, "end": 67, "count": 1 },
  { "start": 68, "end": 68, "count": 2 }]
);

function testCoverageWithSequenceOfBlocks() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);

  builder.addFunction("main", kSig_v_i)
    .addLocals(kWasmI32, 1)
    .addBody([
//------------------------------------
      ...wasmI32Const(1),       //  3
      kExprDrop,                //  5
      kExprBlock, kWasmVoid,    //  6
        ...wasmI32Const(2),     //  8
        kExprDrop,              // 10
        kExprLocalGet, 0,       // 11
        kExprBrIf, 0,           // 13
//------------------------------------
        ...wasmI32Const(3),     // 15
        kExprDrop,              // 17
      kExprEnd,                 // 18
//------------------------------------
      kExprBlock, kWasmVoid,    // 19
        ...wasmI32Const(4),     // 21
        kExprDrop,              // 23
        kExprLocalGet, 0,       // 24
        kExprBrIf, 0,           // 26
//------------------------------------
        ...wasmI32Const(5),     // 28
        kExprDrop,              // 30
      kExprEnd,                 // 31
//------------------------------------
      ...wasmI32Const(6),       // 32
      kExprDrop,                // 34
    ])                          // 35
//------------------------------------
    .exportAs("main");

  instance = builder.instantiate({});
  instance.exports.main(1);
}
TestCoverage(
  "test coverage with sequence of blocks",
  testCoverageWithSequenceOfBlocks.toString(),
  [{ "start": 0, "end": 14, "count": 1 },
  { "start": 15, "end": 18, "count": 0 },
  { "start": 19, "end": 27, "count": 1 },
  { "start": 28, "end": 31, "count": 0 },
  { "start": 32, "end": 35, "count": 1 }]
);

function testCoverageWithMultipleFunctionsAndImportedFunctions() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  // Function 0
  builder.addImport("o", "fn", kSig_i_r);

  // Function 1
  builder.addFunction("square", kSig_i_i)
    .addBody([
//--------------------------------
      kExprLocalGet, 0,      //  1
      kExprLocalGet, 0,      //  3
      kExprI32Mul,           //  5
    ]);                      //  6
//--------------------------------

  builder.addFunction("main", kSig_i_i)
    .addBody([
//--------------------------------
      kExprLocalGet, 0,      //  9
      kExprCallFunction, 1,  // 11
//--------------------------------
    ])                       // 13
//--------------------------------
    .exportAs("main");

  instance = builder.instantiate({ o: { fn: function() {} } });
  let result = instance.exports.main(11);
  assertEquals(121, result);
}
TestCoverage(
  "test coverage with multiple functions and imported functions",
  testCoverageWithMultipleFunctionsAndImportedFunctions.toString(),
  [
    // Function 1
    { "start": 0, "end": 6, "count": 1 },
    // Function 2
    { "start": 8, "end": 12, "count": 1 },
    { "start": 13, "end": 13, "count": 1 },
  ]
);
