// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function bytes() {
  var buffer = new ArrayBuffer(arguments.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < arguments.length; i++) {
    var val = arguments[i];
    if ((typeof val) == "string") val = val.charCodeAt(0);
    view[i] = val | 0;
  }
  return buffer;
}

// Header declaration constants
var kWasmH0 = 0;
var kWasmH1 = 0x61;
var kWasmH2 = 0x73;
var kWasmH3 = 0x6d;

var kWasmV0 = 0xC;
var kWasmV1 = 0;
var kWasmV2 = 0;
var kWasmV3 = 0;

var kHeaderSize = 8;
var kPageSize = 65536;

function bytesWithHeader() {
  var buffer = new ArrayBuffer(kHeaderSize + arguments.length);
  var view = new Uint8Array(buffer);
  view[0] = kWasmH0;
  view[1] = kWasmH1;
  view[2] = kWasmH2;
  view[3] = kWasmH3;
  view[4] = kWasmV0;
  view[5] = kWasmV1;
  view[6] = kWasmV2;
  view[7] = kWasmV3;
  for (var i = 0; i < arguments.length; i++) {
    var val = arguments[i];
    if ((typeof val) == "string") val = val.charCodeAt(0);
    view[kHeaderSize + i] = val | 0;
  }
  return buffer;
}

var kDeclNoLocals = 0;

// Section declaration constants
var kUnknownSectionCode = 0;
var kTypeSectionCode = 1;      // Function signature declarations
var kImportSectionCode = 2;    // Import declarations
var kFunctionSectionCode = 3;  // Function declarations
var kTableSectionCode = 4;     // Indirect function table and other tables
var kMemorySectionCode = 5;    // Memory attributes
var kGlobalSectionCode = 6;    // Global declarations
var kExportSectionCode = 7;    // Exports
var kStartSectionCode = 8;     // Start function declaration
var kElementSectionCode = 9;  // Elements section
var kCodeSectionCode = 10;      // Function code
var kDataSectionCode = 11;     // Data segments
var kNameSectionCode = 12;     // Name section (encoded as string)

var kWasmFunctionTypeForm = 0x40;
var kWasmAnyFunctionTypeForm = 0x20;

var kResizableMaximumFlag = 1;

// Function declaration flags
var kDeclFunctionName   = 0x01;
var kDeclFunctionImport = 0x02;
var kDeclFunctionLocals = 0x04;
var kDeclFunctionExport = 0x08;

// Local types
var kAstStmt = 0;
var kAstI32 = 1;
var kAstI64 = 2;
var kAstF32 = 3;
var kAstF64 = 4;

var kExternalFunction = 0;
var kExternalTable = 1;
var kExternalMemory = 2;
var kExternalGlobal = 3;

// Useful signatures
var kSig_i = makeSig([], [kAstI32]);
var kSig_d = makeSig([], [kAstF64]);
var kSig_i_i = makeSig([kAstI32], [kAstI32]);
var kSig_i_l = makeSig([kAstI64], [kAstI32]);
var kSig_i_ii = makeSig([kAstI32, kAstI32], [kAstI32]);
var kSig_i_iii = makeSig([kAstI32, kAstI32, kAstI32], [kAstI32]);
var kSig_d_dd = makeSig([kAstF64, kAstF64], [kAstF64]);
var kSig_l_ll = makeSig([kAstI64, kAstI64], [kAstI64]);
var kSig_i_dd = makeSig([kAstF64, kAstF64], [kAstI32]);
var kSig_v_v = makeSig([], []);
var kSig_i_v = makeSig([], [kAstI32]);
var kSig_v_i = makeSig([kAstI32], []);
var kSig_v_ii = makeSig([kAstI32, kAstI32], []);
var kSig_v_iii = makeSig([kAstI32, kAstI32, kAstI32], []);
var kSig_v_d = makeSig([kAstF64], []);
var kSig_v_dd = makeSig([kAstF64, kAstF64], []);
var kSig_v_ddi = makeSig([kAstF64, kAstF64, kAstI32], []);

function makeSig(params, results) {
  return {params: params, results: results};
}

function makeSig_v_x(x) {
  return makeSig([x], []);
}

function makeSig_v_xx(x) {
  return makeSig([x, x], []);
}

function makeSig_r_v(r) {
  return makeSig([], [r]);
}

function makeSig_r_x(r, x) {
  return makeSig([x], [r]);
}

function makeSig_r_xx(r, x) {
  return makeSig([x, x], [r]);
}

// Opcodes
var kExprUnreachable = 0x00;
var kExprNop = 0x0a;
var kExprBlock = 0x01;
var kExprLoop = 0x02;
var kExprIf = 0x03;
var kExprElse = 0x04;
var kExprSelect = 0x05;
var kExprBr = 0x06;
var kExprBrIf = 0x07;
var kExprBrTable = 0x08;
var kExprReturn = 0x09;
var kExprThrow = 0xfa;
var kExprTry = 0xfb;
var kExprCatch = 0xfe;
var kExprEnd = 0x0f;
var kExprTeeLocal = 0x19;
var kExprDrop = 0x0b;

var kExprI32Const = 0x10;
var kExprI64Const = 0x11;
var kExprF64Const = 0x12;
var kExprF32Const = 0x13;
var kExprGetLocal = 0x14;
var kExprSetLocal = 0x15;
var kExprCallFunction = 0x16;
var kExprCallIndirect = 0x17;
var kExprI8Const = 0xcb;
var kExprGetGlobal = 0xbb;
var kExprSetGlobal = 0xbc;

var kExprI32LoadMem8S = 0x20;
var kExprI32LoadMem8U = 0x21;
var kExprI32LoadMem16S = 0x22;
var kExprI32LoadMem16U = 0x23;
var kExprI64LoadMem8S = 0x24;
var kExprI64LoadMem8U = 0x25;
var kExprI64LoadMem16S = 0x26;
var kExprI64LoadMem16U = 0x27;
var kExprI64LoadMem32S = 0x28;
var kExprI64LoadMem32U = 0x29;
var kExprI32LoadMem = 0x2a;
var kExprI64LoadMem = 0x2b;
var kExprF32LoadMem = 0x2c;
var kExprF64LoadMem = 0x2d;

var kExprI32StoreMem8 = 0x2e;
var kExprI32StoreMem16 = 0x2f;
var kExprI64StoreMem8 = 0x30;
var kExprI64StoreMem16 = 0x31;
var kExprI64StoreMem32 = 0x32;
var kExprI32StoreMem = 0x33;
var kExprI64StoreMem = 0x34;
var kExprF32StoreMem = 0x35;
var kExprF64StoreMem = 0x36;

var kExprMemorySize = 0x3b;
var kExprGrowMemory = 0x39;

var kExprI32Add = 0x40;
var kExprI32Sub = 0x41;
var kExprI32Mul = 0x42;
var kExprI32DivS = 0x43;
var kExprI32DivU = 0x44;
var kExprI32RemS = 0x45;
var kExprI32RemU = 0x46;
var kExprI32And = 0x47;
var kExprI32Ior = 0x48;
var kExprI32Xor = 0x49;
var kExprI32Shl = 0x4a;
var kExprI32ShrU = 0x4b;
var kExprI32ShrS = 0x4c;
var kExprI32Eq = 0x4d;
var kExprI32Ne = 0x4e;
var kExprI32LtS = 0x4f;
var kExprI32LeS = 0x50;
var kExprI32LtU = 0x51;
var kExprI32LeU = 0x52;
var kExprI32GtS = 0x53;
var kExprI32GeS = 0x54;
var kExprI32GtU = 0x55;
var kExprI32GeU = 0x56;
var kExprI32Clz = 0x57;
var kExprI32Ctz = 0x58;
var kExprI32Popcnt = 0x59;
var kExprI32Eqz = 0x5a;
var kExprI64Add = 0x5b;
var kExprI64Sub = 0x5c;
var kExprI64Mul = 0x5d;
var kExprI64DivS = 0x5e;
var kExprI64DivU = 0x5f;
var kExprI64RemS = 0x60;
var kExprI64RemU = 0x61;
var kExprI64And = 0x62;
var kExprI64Ior = 0x63;
var kExprI64Xor = 0x64;
var kExprI64Shl = 0x65;
var kExprI64ShrU = 0x66;
var kExprI64ShrS = 0x67;
var kExprI64Eq = 0x68;
var kExprI64Ne = 0x69;
var kExprI64LtS = 0x6a;
var kExprI64LeS = 0x6b;
var kExprI64LtU = 0x6c;
var kExprI64LeU = 0x6d;
var kExprI64GtS = 0x6e;
var kExprI64GeS = 0x6f;
var kExprI64GtU = 0x70;
var kExprI64GeU = 0x71;
var kExprI64Clz = 0x72;
var kExprI64Ctz = 0x73;
var kExprI64Popcnt = 0x74;
var kExprF32Add = 0x75;
var kExprF32Sub = 0x76;
var kExprF32Mul = 0x77;
var kExprF32Div = 0x78;
var kExprF32Min = 0x79;
var kExprF32Max = 0x7a;
var kExprF32Abs = 0x7b;
var kExprF32Neg = 0x7c;
var kExprF32CopySign = 0x7d;
var kExprF32Ceil = 0x7e;
var kExprF32Floor = 0x7f;
var kExprF32Trunc = 0x80;
var kExprF32NearestInt = 0x81;
var kExprF32Sqrt = 0x82;
var kExprF32Eq = 0x83;
var kExprF32Ne = 0x84;
var kExprF32Lt = 0x85;
var kExprF32Le = 0x86;
var kExprF32Gt = 0x87;
var kExprF32Ge = 0x88;
var kExprF64Add = 0x89;
var kExprF64Sub = 0x8a;
var kExprF64Mul = 0x8b;
var kExprF64Div = 0x8c;
var kExprF64Min = 0x8d;
var kExprF64Max = 0x8e;
var kExprF64Abs = 0x8f;
var kExprF64Neg = 0x90;
var kExprF64CopySign = 0x91;
var kExprF64Ceil = 0x92;
var kExprF64Floor = 0x93;
var kExprF64Trunc = 0x94;
var kExprF64NearestInt = 0x95;
var kExprF64Sqrt = 0x96;
var kExprF64Eq = 0x97;
var kExprF64Ne = 0x98;
var kExprF64Lt = 0x99;
var kExprF64Le = 0x9a;
var kExprF64Gt = 0x9b;
var kExprF64Ge = 0x9c;
var kExprI32SConvertF32 = 0x9d;
var kExprI32SConvertF64 = 0x9e;
var kExprI32UConvertF32 = 0x9f;
var kExprI32UConvertF64 = 0xa0;
var kExprI32ConvertI64 = 0xa1;
var kExprI64SConvertF32 = 0xa2;
var kExprI64SConvertF64 = 0xa3;
var kExprI64UConvertF32 = 0xa4;
var kExprI64UConvertF64 = 0xa5;
var kExprI64SConvertI32 = 0xa6;
var kExprI64UConvertI32 = 0xa7;
var kExprF32SConvertI32 = 0xa8;
var kExprF32UConvertI32 = 0xa9;
var kExprF32SConvertI64 = 0xaa;
var kExprF32UConvertI64 = 0xab;
var kExprF32ConvertF64 = 0xac;
var kExprF32ReinterpretI32 = 0xad;
var kExprF64SConvertI32 = 0xae;
var kExprF64UConvertI32 = 0xaf;
var kExprF64SConvertI64 = 0xb0;
var kExprF64UConvertI64 = 0xb1;
var kExprF64ConvertF32 = 0xb2;
var kExprF64ReinterpretI64 = 0xb3;
var kExprI32ReinterpretF32 = 0xb4;
var kExprI64ReinterpretF64 = 0xb5;
var kExprI32Ror = 0xb6;
var kExprI32Rol = 0xb7;
var kExprI64Ror = 0xb8;
var kExprI64Rol = 0xb9;

var kTrapUnreachable          = 0;
var kTrapMemOutOfBounds       = 1;
var kTrapDivByZero            = 2;
var kTrapDivUnrepresentable   = 3;
var kTrapRemByZero            = 4;
var kTrapFloatUnrepresentable = 5;
var kTrapFuncInvalid          = 6;
var kTrapFuncSigMismatch      = 7;
var kTrapInvalidIndex         = 8;

var kTrapMsgs = [
  "unreachable",
  "memory access out of bounds",
  "divide by zero",
  "divide result unrepresentable",
  "remainder by zero",
  "integer result unrepresentable",
  "invalid function",
  "function signature mismatch",
  "invalid index into function table"
];

function assertTraps(trap, code) {
    var threwException = true;
    try {
      if (typeof code === 'function') {
        code();
      } else {
        eval(code);
      }
      threwException = false;
    } catch (e) {
      assertEquals("object", typeof e);
      assertEquals(kTrapMsgs[trap], e.message);
      // Success.
      return;
    }
    throw new MjsUnitAssertionError("Did not trap, expected: " + kTrapMsgs[trap]);
}

function assertWasmThrows(value, code) {
    assertEquals("number", typeof(value));
    try {
      if (typeof code === 'function') {
        code();
      } else {
        eval(code);
      }
    } catch (e) {
      assertEquals("number", typeof e);
      assertEquals(value, e);
      // Success.
      return;
    }
    throw new MjsUnitAssertionError("Did not throw at all, expected: " + value);
}
