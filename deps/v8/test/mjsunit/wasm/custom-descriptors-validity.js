// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-shared

// At the time of writing, these tests cover the same cases as:
// https://github.com/WebAssembly/binaryen/pull/7392/files

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function Build(...recgroups) {
  let builder = new WasmModuleBuilder();
  for (let r of recgroups) {
    builder.startRecGroup();
    r(builder);
    builder.endRecGroup();
  }
  return builder;
}

function CheckValid(...recgroups) {
  let builder = Build(...recgroups);
  builder.instantiate();
}

function CheckInvalid(message, ...recgroups) {
  let builder = Build(...recgroups);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError, message);
}

CheckValid((builder) => {
  builder.addStruct({descriptor: 1});
  builder.addStruct({describes: 0});
});

CheckInvalid(/Type 0 has descriptor 1 but 1 doesn't describe 0/, (builder) => {
  builder.addStruct({descriptor: 1});
  builder.addStruct({});
});

CheckInvalid(/Type 1 describes 0 but 0 isn't a descriptor for 1/, (builder) => {
  builder.addStruct({});
  builder.addStruct({describes: 0});
});

CheckInvalid(/types can only describe previously-declared types/, (builder) => {
  builder.addStruct({describes: 1});
  builder.addStruct({descriptor: 0});
});

// Defining an array type with a descriptor is so utterly invalid that we
// don't even have module builder support for it.
assertThrows(() => {
  let bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d,  // wasm magic
      0x01, 0x00, 0x00, 0x00,  // wasm version

      0x01,                    // section kind: Type
      0x10,                    // section length 16
      0x01, 0x4e,              // types count 1: rec. group definition
      0x02,                    // recursive group size 2
      0x50, 0x00,              //  subtype extensible, supertype count 0
      0x4d, 0x01,              //  described_by  $type1
      0x5e, 0x78, 0x00,        //  kind: array i8 immutable
      0x50, 0x00,              //  subtype extensible, supertype count 0
      0x4c, 0x00,              //  describes  $type0
      0x5f, 0x00,              //  kind: struct, field count 0
                               // end of rec. group
  ]);
  new WebAssembly.Instance(new WebAssembly.Module(bytes));
}, WebAssembly.CompileError, /'descriptor' may only be used with structs/);

// Defining an array type as a descriptor is so utterly invalid that we
// don't even have module builder support for it.
assertThrows(() => {
  let bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d,  // wasm magic
      0x01, 0x00, 0x00, 0x00,  // wasm version

      0x01,                    // section kind: Type
      0x10,                    // section length 16
      0x01, 0x4e,              // types count 1: rec. group definition
      0x02,                    // recursive group size 2
      0x50, 0x00,              //  subtype extensible, supertype count 0
      0x4d, 0x01,              //  described_by  $type1
      0x5f, 0x00,              //  kind: struct, field count 0
      0x50, 0x00,              //  subtype extensible, supertype count 0
      0x4c, 0x00,              //  describes  $type0
      0x5e, 0x78, 0x00,        //  kind: array i8 immutable
                               // end of rec. group
  ]);
  new WebAssembly.Instance(new WebAssembly.Module(bytes));
}, WebAssembly.CompileError, /'describes' may only be used with structs/);

CheckValid((builder) => {
  builder.addStruct({descriptor: 1, shared: true});
  builder.addStruct({describes: 0, shared: true});
});

CheckInvalid(/Type 0 and its descriptor 1 must have same sharedness/,
             (builder) => {
  builder.addStruct({descriptor: 1, shared: true});
  builder.addStruct({describes: 0});
});

// Shared descriptors of non-shared types could technically be allowed. TBD.
CheckInvalid(/Type 0 and its descriptor 1 must have same sharedness/,
             (builder) => {
  builder.addStruct({descriptor: 1});
  builder.addStruct({describes: 0, shared: true});
});

CheckValid((builder) => {
  builder.addStruct({descriptor: 1, final: false});  // 0
  builder.addStruct({describes: 0, final: false});   // 1
}, (builder) => {
  builder.addStruct({descriptor: 3, supertype: 0});  // 2
  builder.addStruct({describes: 2, supertype: 1});   // 3
});

CheckValid((builder) => {
  builder.addStruct({final: false});  // 0
}, (builder) => {
  builder.addStruct({descriptor: 2});  // 1
  builder.addStruct({describes: 1});   // 2
}, (builder) => {
  builder.addStruct({descriptor: 4, supertype: 0});  // 3
  builder.addStruct({describes: 3, supertype: 2});   // 4
});

CheckInvalid(/type 3 has invalid explicit supertype 1/, (builder) => {
  builder.addStruct({final: false});  // 0
  builder.addStruct({final: false});  // 1
}, (builder) => {
  builder.addStruct({supertype: 0, descriptor: 3});  // 2
  builder.addStruct({supertype: 1, describes: 2});   // 3
});

CheckInvalid(/type 4 has invalid explicit supertype 2/, (builder) => {
  builder.addStruct({descriptor: 1});  // 0
  builder.addStruct({describes: 0});   // 1
  builder.addStruct({descriptor: 3, final: false});  // 2
  builder.addStruct({describes: 2, final: false});   // 3
}, (builder) => {
  builder.addStruct({supertype: 2, descriptor: 5});  // 4
  builder.addStruct({supertype: 1, describes: 4});   // 5
});

CheckInvalid(/type 2 has invalid explicit supertype 0/, (builder) => {
  builder.addStruct({descriptor: 1, final: false});  // 0
  builder.addStruct({describes: 0});                 // 1
}, (builder) => {
  builder.addStruct({supertype: 0});  // 2
});

CheckInvalid(/type 2 has invalid explicit supertype 1/, (builder) => {
  builder.addStruct({descriptor: 1});               // 0
  builder.addStruct({describes: 0, final: false});  // 1
}, (builder) => {
  builder.addStruct({supertype: 1});  // 2
});
