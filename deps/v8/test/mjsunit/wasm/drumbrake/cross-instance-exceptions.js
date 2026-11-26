// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testCrossInstanceIndirectCallExceptionIsCaught() {
  const builderB = new WasmModuleBuilder();
  const signatureB = builderB.addType(kSig_v_v);
  const tagB = builderB.addTag(kSig_v_v);
  const thrower = builderB.addFunction("thrower", signatureB)
      .addBody([kExprThrow, tagB]);
  const tableB = builderB.addTable(kWasmFuncRef, 1, 1);
  builderB.addActiveElementSegment(
      tableB.index, wasmI32Const(0), [[kExprRefFunc, thrower.index]],
      kWasmFuncRef);
  builderB.addExportOfKind("table", kExternalTable, tableB.index);
  const instanceB = builderB.instantiate();

  const builderA = new WasmModuleBuilder();
  const signatureA = builderA.addType(kSig_v_v);
  builderA.addImportedTable("m", "table", 1, 1, kWasmFuncRef);
  builderA.addFunction("main", kSig_i_v)
      .addBody([
        kExprTry, kWasmI32,
          kExprI32Const, 0,
          kExprCallIndirect, signatureA, 0,
          ...wasmI32Const(1000),
        kExprCatchAll,
          ...wasmI32Const(99),
        kExprEnd,
      ])
      .exportFunc();

  const instanceA = builderA.instantiate({m: {table: instanceB.exports.table}});
  assertEquals(99, instanceA.exports.main());
})();

function testCrossInstanceExceptions(targetKind) {
  const jsTarget = targetKind !== "wasm";
  const jsError = targetKind === "js-error";
  const tagSignature = makeSig(
      jsError ? [kWasmExternRef] : [kWasmI32, kWasmExternRef], []);
  for (const callRef of [false, true]) {
    for (const tailCall of [false, true]) {
      for (const matchingCatch of [false, true]) {
        const builderB = new WasmModuleBuilder();
        const signatureB = builderB.addType(kSig_i_v);
        const tagB = jsError ?
            builderB.addImportedTag("m", "tag", tagSignature) :
            builderB.addTag(tagSignature);
        const mode = builderB.addGlobal(kWasmI32, true, false, wasmI32Const(1))
            .exportAs("mode");
        const payloadGlobal = builderB.addGlobal(kWasmExternRef, true)
            .exportAs("payload");
        const throwerIndex = jsTarget ?
            builderB.addImport("m", "thrower", signatureB) :
            builderB.addFunction("thrower", signatureB)
                .addBody([
                  kExprGlobalGet, mode.index,
                  kExprIf, kWasmVoid,
                    kExprGlobalGet, mode.index, kExprI32Const, 2, kExprI32Eq,
                    kExprIf, kWasmVoid,
                      kExprUnreachable,
                    kExprEnd,
                    ...wasmI32Const(123),
                    kExprGlobalGet, payloadGlobal.index,
                    kExprThrow, tagB,
                  kExprEnd,
                  ...wasmI32Const(42),
                ]).index;
        builderB.addFunction("trap", signatureB)
            .addBody([kExprUnreachable]).exportFunc();
        const tableB = builderB.addTable(wasmRefNullType(signatureB), 1, 1);
        builderB.addActiveElementSegment(
            tableB.index, wasmI32Const(0), [[kExprRefFunc, throwerIndex]],
            wasmRefNullType(signatureB));
        builderB.addExportOfKind("table", kExternalTable, tableB.index);
        builderB.addExportOfKind("tag", kExternalTag, tagB);
        const error = new Error("cross-instance JS exception");
        const payload = {value: "exception payload"};
        const instanceB = builderB.instantiate({m: {
          tag: WebAssembly.JSTag,
          thrower() {
            // Reenter the suspended caller and collect before throwing.
            instanceA.exports.noop();
            gc();
            if (instanceB.exports.mode.value === 0) return 42;
            if (instanceB.exports.mode.value === 2) {
              return instanceB.exports.trap();
            }
            if (jsError) throw error;
            throw new WebAssembly.Exception(
                instanceB.exports.tag, [123, payload]);
          },
        }});
        instanceB.exports.payload.value = payload;

        const builderA = new WasmModuleBuilder();
        const signatureA = builderA.addType(kSig_i_v);
        const importedTag = builderA.addImportedTag("m", "tag", tagSignature);
        const differentTag = builderA.addTag(tagSignature);
        const caughtRef = builderA.addGlobal(kWasmExternRef, true)
            .exportAs("caughtRef");
        const caughtNumber = builderA.addGlobal(kWasmI32, true)
            .exportAs("caughtNumber");
        const savePayload = [
          kExprGlobalSet, caughtRef.index,
          ...(jsError ? [] : [kExprGlobalSet, caughtNumber.index]),
        ];
        builderA.addImportedTable(
            "m", "table", 1, 1, wasmRefNullType(signatureA));
        const call = callRef ? [
          kExprI32Const, 0, kExprTableGet, 0,
          tailCall ? kExprReturnCallRef : kExprCallRef, signatureA,
        ] : [
          kExprI32Const, 0,
          tailCall ? kExprReturnCallIndirect : kExprCallIndirect, signatureA, 0,
        ];
        const inner = builderA.addFunction("inner", kSig_i_v)
            .addBody([
              kExprTry, kWasmI32,
                ...call,
              kExprCatch, matchingCatch ? importedTag : differentTag,
                ...savePayload,
                ...wasmI32Const(99),
              kExprEnd,
            ])
            .exportFunc();
        // Unwinding must also cross a frame with no exception handler.
        const middle = builderA.addFunction("middle", kSig_i_v)
            .addBody([kExprCallFunction, inner.index]);
        builderA.addFunction("outer", kSig_i_v)
            .addBody([
              kExprTry, kWasmI32,
                kExprCallFunction, middle.index,
              kExprCatch, importedTag,
                ...savePayload,
                ...wasmI32Const(77),
              kExprEnd,
            ])
            .exportFunc();
        builderA.addFunction("catchAll", kSig_i_v)
            .addBody([
              kExprTry, kWasmI32,
                kExprCallFunction, middle.index,
              kExprCatchAll,
                ...wasmI32Const(77),
              kExprEnd,
            ])
            .exportFunc();
        builderA.addFunction("noop", kSig_v_v).addBody([]).exportFunc();
        const instanceA = builderA.instantiate({m: {
          table: instanceB.exports.table, tag: instanceB.exports.tag,
        }});
        const caughtLocally = !tailCall && matchingCatch;
        assertEquals(caughtLocally ? 99 : 77, instanceA.exports.outer());
        assertSame(jsError ? error : payload, instanceA.exports.caughtRef.value);
        if (!jsError) {
          assertEquals(123, instanceA.exports.caughtNumber.value);
        }
        assertEquals(caughtLocally ? 99 : 77, instanceA.exports.catchAll());
        if (caughtLocally) {
          assertEquals(99, instanceA.exports.inner());
        } else if (jsError) {
          assertThrowsEquals(() => instanceA.exports.inner(), error);
        } else {
          assertThrows(() => {
            try {
              instanceA.exports.inner();
            } catch (exception) {
              assertTrue(exception.is(instanceB.exports.tag));
              assertEquals(123, exception.getArg(instanceB.exports.tag, 0));
              assertSame(payload, exception.getArg(instanceB.exports.tag, 1));
              throw exception;
            }
          }, WebAssembly.Exception);
        }
        for (const call of [
          instanceA.exports.inner, instanceA.exports.outer,
          instanceA.exports.catchAll,
        ]) {
          instanceB.exports.mode.value = 0;
          assertEquals(42, call());
          instanceB.exports.mode.value = 2;
          assertTraps(kTrapUnreachable, call);
          instanceB.exports.mode.value = 0;
          assertEquals(42, call());
        }
      }
    }
  }
}

testCrossInstanceExceptions("wasm");
testCrossInstanceExceptions("js-wasm-exception");
testCrossInstanceExceptions("js-error");
