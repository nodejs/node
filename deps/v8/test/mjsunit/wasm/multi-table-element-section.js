// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref

load("test/mjsunit/wasm/wasm-module-builder.js");

const value1 = 11;
const value2 = 22;
const value3 = 46;
const value4 = 57;
const value5 = 13;

// The offsets for the initialization of tables. The segement for table2 should
// overlap with the segment of table1, because table2 is actually the imported
// table1.
const offset1 = 2;
const offset2 = offset1 + 1;
const offset3 = 4;
const offset4 = 1;

const instance_for_import = (function() {
    const builder_for_import = new WasmModuleBuilder();
    const t1 = builder_for_import.addTable(kWasmAnyFunc, 15, 15)
        .exportAs("table").index;
    const f1 = builder_for_import.addFunction('f1', kSig_i_v)
        .addBody([kExprI32Const, value1]).index;
    const f2 = builder_for_import.addFunction('f2', kSig_i_v)
        .addBody([kExprI32Const, value2]).index;

    builder_for_import.addElementSegment(t1, offset1, false, [f1, f2], false);
    const instance_for_import = builder_for_import.instantiate();
    const table1 = instance_for_import.exports.table;
    assertEquals(value1, table1.get(offset1)());
    assertEquals(value2, table1.get(offset1 + 1)());
    return instance_for_import;
})();

const builder = new WasmModuleBuilder();

const t2 = builder.addImportedTable("exports", "table", 15, 15);
builder.addExportOfKind("table2", kExternalTable, t2);
const t3 = builder.addTable(kWasmAnyFunc, 10).exportAs("table3").index;
const t4 = builder.addTable(kWasmAnyFunc, 12).exportAs("table4").index;

const f3 = builder.addFunction('f3', kSig_i_v)
                  .addBody([kExprI32Const, value3]).index;
const f4 = builder.addFunction('f4', kSig_i_v)
                  .addBody([kExprI32Const, value4]).index;
const f5 = builder.addFunction('f5', kSig_i_v)
                  .addBody([kExprI32Const, value5]).index;


builder.addElementSegment(t2, offset2, false, [f3, f4], false);
builder.addElementSegment(t3, offset3, false, [f5, f4], false);
builder.addElementSegment(t4, offset4, false, [f3, f5], false);
// Add one more overlapping offset
builder.addElementSegment(t4, offset4 + 1, false, [f4, f3], false);

const instance = builder.instantiate(instance_for_import);
// table2 == table1
const table2 = instance.exports.table2;
const table3 = instance.exports.table3;
const table4 = instance.exports.table4;
// table1 == table2
assertEquals(value1, table2.get(offset1)());
assertEquals(value3, table2.get(offset2)());
assertEquals(value4, table2.get(offset2 + 1)());

assertEquals(value5, table3.get(offset3)());
assertEquals(value4, table3.get(offset3 + 1)());

assertEquals(value3, table4.get(offset4)());
assertEquals(value4, table4.get(offset4 + 1)());
assertEquals(value3, table4.get(offset4 + 2)());
