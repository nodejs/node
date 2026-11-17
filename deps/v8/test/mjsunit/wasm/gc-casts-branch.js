// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

// Type checks against an rtt (br_on_cast, br_on_cast_fail, ref.test) whose
// result is only used by a branch are lowered directly into that branch by
// the optimizing compiler. Check all these forms against the value form, for
// various source and target types.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $base = builder.addStruct([makeField(kWasmI32, true)]);
let $sub1 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], supertype: $base});
let $sub2 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], supertype: $sub1});
// At depth 3, the supertype array of the object's type is bounds-checked.
let $sub3 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], supertype: $sub2});
let $sub4 = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], supertype: $sub3});
let $final = builder.addStruct(
    {fields: [makeField(kWasmI32, true)], supertype: $base, final: true});
let $other = builder.addStruct([makeField(kWasmI64, true)]);
let $array = builder.addArray(kWasmI32, {mutable: true});
// Types with custom descriptors: exact casts check the immediate supertype of
// the object's map.
let $desc = builder.nextTypeIndex() + 1;
let $described = builder.addStruct({fields: [], descriptor: $desc});
builder.addStruct({fields: [], describes: $described});
let $desc_sub = builder.nextTypeIndex() + 1;
let $described_sub = builder.addStruct(
    {fields: [], supertype: $described, descriptor: $desc_sub});
builder.addStruct({fields: [], supertype: $desc, describes: $described_sub});
builder.endRecGroup();

let $nop = builder.addImport('m', 'nop', kSig_v_v);

// The supertypes of each struct type (including itself).
let supertypes = {
  [$base]: [$base],
  [$sub1]: [$sub1, $base],
  [$sub2]: [$sub2, $sub1, $base],
  [$sub3]: [$sub3, $sub2, $sub1, $base],
  [$sub4]: [$sub4, $sub3, $sub2, $sub1, $base],
  [$final]: [$final, $base],
  [$other]: [$other],
  [$array]: [$array],
  [$described]: [$described],
  [$described_sub]: [$described_sub, $described],
};

// Functions creating the Wasm values.
for (let type of [$base, $sub1, $sub2, $sub3, $sub4, $final]) {
  builder.addFunction('make' + type, makeSig([], [kWasmAnyRef]))
      .exportFunc()
      .addBody([kExprI32Const, 7, kGCPrefix, kExprStructNew, type]);
}
builder.addFunction('make' + $other, makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([kExprI64Const, 7, kGCPrefix, kExprStructNew, $other]);
builder.addFunction('make' + $array, makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([kExprI32Const, 7, kGCPrefix, kExprArrayNewFixed, $array, 1]);
builder.addFunction('make' + $described, makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([
      kGCPrefix, kExprStructNew, $desc,
      kGCPrefix, kExprStructNewDesc, $described,
    ]);
builder.addFunction('make' + $described_sub, makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([
      kGCPrefix, kExprStructNew, $desc_sub,
      kGCPrefix, kExprStructNewDesc, $described_sub,
    ]);
builder.addFunction('makeI31', makeSig([], [kWasmAnyRef]))
    .exportFunc()
    .addBody([kExprI32Const, 42, kGCPrefix, kExprRefI31]);

// Source types: anyref (casts from any also check that the object is a Wasm
// object) and (ref null $base) (the object cannot be an i31).
let sources = [
  {name: 'any', type: wasmRefNullType(kWasmAnyRef), accepts: (v) => true},
  {
    name: 'base',
    type: wasmRefNullType($base),
    accepts: (v) => v.kind == 'null' ||
        (v.kind == 'struct' && supertypes[v.type].includes($base))
  },
];

// Target types, nullable or not, possibly exact.
let targets = [];
for (let type of [$base, $sub1, $sub3, $final]) {
  for (let nullable of [false, true]) targets.push({type, nullable});
}
targets.push({type: $sub3, nullable: false, exact: true});
targets.push({type: $described, nullable: false, exact: true});
targets.push({type: $described, nullable: true, exact: true});
targets.push({type: $described, nullable: false});

function heapType(target) {
  let type = target.nullable ? wasmRefNullType(target.type) :
                               wasmRefType(target.type);
  return target.exact ? type.exact() : type;
}

function refTest(target) {
  return [
    kGCPrefix, target.nullable ? kExprRefTestNull : kExprRefTest,
    ...(target.exact ? [kWasmExact] : []), ...wasmSignedLeb(target.type)
  ];
}

let tests = [];
for (let source of sources) {
  for (let target of targets) {
    // Only the targets that are subtypes of the source.
    if (source.name == 'base' &&
        !supertypes[target.type].includes($base)) {
      continue;
    }
    let name = `${source.name}_${target.type}_${target.nullable ? 'null' : ''}` +
        `${target.exact ? 'exact' : ''}`;
    let sig = makeSig([source.type, kWasmI32], [kWasmI32]);
    let forms = {};
    // ref.test as a value (stored into a local and returned).
    forms.value = builder.addFunction('value_' + name, sig).exportFunc()
        .addLocals(kWasmI32, 1)
        .addBody([
          kExprLocalGet, 0, ...refTest(target), kExprLocalSet, 2,
          kExprLocalGet, 2,
        ]);
    // if on a ref.test.
    forms.if = builder.addFunction('if_' + name, sig).exportFunc()
        .addBody([
          kExprLocalGet, 0, ...refTest(target),
          kExprIf, kWasmI32, kExprI32Const, 1, kExprElse, kExprI32Const, 0,
          kExprEnd,
        ]);
    // A ref.test in a different block from the branch using it.
    forms.separate = builder.addFunction('separate_' + name, sig).exportFunc()
        .addLocals(kWasmI32, 1)
        .addBody([
          kExprLocalGet, 0, ...refTest(target), kExprLocalSet, 2,
          kExprLocalGet, 1, kExprIf, kWasmVoid, kExprCallFunction, $nop,
          kExprEnd,
          kExprLocalGet, 2,
          kExprIf, kWasmI32, kExprI32Const, 1, kExprElse, kExprI32Const, 0,
          kExprEnd,
        ]);
    // br_on_cast.
    forms.br_on_cast = builder.addFunction('br_on_cast_' + name, sig)
        .exportFunc()
        .addBody([
          kExprBlock, kAnyRefCode,
          kExprLocalGet, 0,
          ...wasmBrOnCast(0, source.type, heapType(target)),
          kExprDrop, kExprI32Const, 0, kExprReturn,
          kExprEnd,
          kExprDrop, kExprI32Const, 1,
        ]);
    // br_on_cast_fail.
    forms.br_on_cast_fail = builder.addFunction('br_on_cast_fail_' + name, sig)
        .exportFunc()
        .addBody([
          kExprBlock, kAnyRefCode,
          kExprLocalGet, 0,
          ...wasmBrOnCastFail(0, source.type, heapType(target)),
          kExprDrop, kExprI32Const, 1, kExprReturn,
          kExprEnd,
          kExprDrop, kExprI32Const, 0,
        ]);
    tests.push({name, source, target, forms});
  }
}

let instance = builder.instantiate({m: {nop: () => {}}});
let exports = instance.exports;

let values = [{name: 'null', kind: 'null', value: null}];
values.push({name: 'i31', kind: 'i31', value: exports.makeI31()});
for (let type in supertypes) {
  values.push({
    name: 'struct' + type,
    kind: 'struct',
    type: Number(type),
    value: exports['make' + type]()
  });
}
values.push({name: 'object', kind: 'js', value: {}});
values.push({name: 'string', kind: 'js', value: 'foo'});
values.push({name: 'number', kind: 'js', value: 1.5});

function expected(target, v) {
  if (v.kind == 'null') return target.nullable ? 1 : 0;
  if (v.kind != 'struct') return 0;
  if (target.exact) return v.type == target.type ? 1 : 0;
  return supertypes[v.type].includes(target.type) ? 1 : 0;
}

function runAll() {
  for (let test of tests) {
    for (let v of values) {
      if (!test.source.accepts(v)) continue;
      let want = expected(test.target, v);
      for (let form in test.forms) {
        for (let flag of [0, 1]) {
          assertEquals(
              want, exports[`${form}_${test.name}`](v.value, flag),
              `${form}_${test.name}(${v.name}, ${flag})`);
        }
      }
    }
  }
}

runAll();
