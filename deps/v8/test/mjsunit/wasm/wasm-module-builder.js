// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for encoding f32 and double constants to bits.
let f32_view = new Float32Array(1);
let f32_bytes_view = new Uint8Array(f32_view.buffer);
let f64_view = new Float64Array(1);
let f64_bytes_view = new Uint8Array(f64_view.buffer);

// The bytes function receives one of
//  - several arguments, each of which is either a number or a string of length
//    1; if it's a string, the charcode of the contained character is used.
//  - a single array argument containing the actual arguments
//  - a single string; the returned buffer will contain the char codes of all
//    contained characters.
function bytes(...input) {
  if (input.length == 1 && typeof input[0] == 'array') input = input[0];
  if (input.length == 1 && typeof input[0] == 'string') {
    let len = input[0].length;
    let view = new Uint8Array(len);
    for (let i = 0; i < len; i++) view[i] = input[0].charCodeAt(i);
    return view.buffer;
  }
  let view = new Uint8Array(input.length);
  for (let i = 0; i < input.length; i++) {
    let val = input[i];
    if (typeof val == 'string') {
      assertEquals(1, val.length, 'string inputs must have length 1');
      val = val.charCodeAt(0);
    }
    view[i] = val | 0;
  }
  return view.buffer;
}

// Header declaration constants
var kWasmH0 = 0;
var kWasmH1 = 0x61;
var kWasmH2 = 0x73;
var kWasmH3 = 0x6d;

var kWasmV0 = 0x1;
var kWasmV1 = 0;
var kWasmV2 = 0;
var kWasmV3 = 0;

var kHeaderSize = 8;
var kPageSize = 65536;
var kSpecMaxPages = 65535;
var kMaxVarInt32Size = 5;
var kMaxVarInt64Size = 10;

let kDeclNoLocals = 0;

// Section declaration constants
let kUnknownSectionCode = 0;
let kTypeSectionCode = 1;        // Function signature declarations
let kImportSectionCode = 2;      // Import declarations
let kFunctionSectionCode = 3;    // Function declarations
let kTableSectionCode = 4;       // Indirect function table and other tables
let kMemorySectionCode = 5;      // Memory attributes
let kGlobalSectionCode = 6;      // Global declarations
let kExportSectionCode = 7;      // Exports
let kStartSectionCode = 8;       // Start function declaration
let kElementSectionCode = 9;     // Elements section
let kCodeSectionCode = 10;       // Function code
let kDataSectionCode = 11;       // Data segments
let kDataCountSectionCode = 12;  // Data segment count (between Element & Code)
let kExceptionSectionCode = 13;  // Exception section (between Global & Export)

// Name section types
let kModuleNameCode = 0;
let kFunctionNamesCode = 1;
let kLocalNamesCode = 2;

let kWasmFunctionTypeForm = 0x60;
let kWasmAnyFunctionTypeForm = 0x70;

let kHasMaximumFlag = 1;
let kSharedHasMaximumFlag = 3;

// Segment flags
let kActiveNoIndex = 0;
let kPassive = 1;
let kActiveWithIndex = 2;

// Function declaration flags
let kDeclFunctionName   = 0x01;
let kDeclFunctionImport = 0x02;
let kDeclFunctionLocals = 0x04;
let kDeclFunctionExport = 0x08;

// Local types
let kWasmStmt = 0x40;
let kWasmI32 = 0x7f;
let kWasmI64 = 0x7e;
let kWasmF32 = 0x7d;
let kWasmF64 = 0x7c;
let kWasmS128 = 0x7b;
let kWasmAnyRef = 0x6f;
let kWasmAnyFunc = 0x70;
let kWasmExnRef = 0x68;

let kExternalFunction = 0;
let kExternalTable = 1;
let kExternalMemory = 2;
let kExternalGlobal = 3;
let kExternalException = 4;

let kTableZero = 0;
let kMemoryZero = 0;
let kSegmentZero = 0;

let kExceptionAttribute = 0;

// Useful signatures
let kSig_i_i = makeSig([kWasmI32], [kWasmI32]);
let kSig_l_l = makeSig([kWasmI64], [kWasmI64]);
let kSig_i_l = makeSig([kWasmI64], [kWasmI32]);
let kSig_i_ii = makeSig([kWasmI32, kWasmI32], [kWasmI32]);
let kSig_i_iii = makeSig([kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
let kSig_v_iiii = makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32], []);
let kSig_f_ff = makeSig([kWasmF32, kWasmF32], [kWasmF32]);
let kSig_d_dd = makeSig([kWasmF64, kWasmF64], [kWasmF64]);
let kSig_l_ll = makeSig([kWasmI64, kWasmI64], [kWasmI64]);
let kSig_i_dd = makeSig([kWasmF64, kWasmF64], [kWasmI32]);
let kSig_v_v = makeSig([], []);
let kSig_i_v = makeSig([], [kWasmI32]);
let kSig_l_v = makeSig([], [kWasmI64]);
let kSig_f_v = makeSig([], [kWasmF32]);
let kSig_d_v = makeSig([], [kWasmF64]);
let kSig_v_i = makeSig([kWasmI32], []);
let kSig_v_ii = makeSig([kWasmI32, kWasmI32], []);
let kSig_v_iii = makeSig([kWasmI32, kWasmI32, kWasmI32], []);
let kSig_v_l = makeSig([kWasmI64], []);
let kSig_v_d = makeSig([kWasmF64], []);
let kSig_v_dd = makeSig([kWasmF64, kWasmF64], []);
let kSig_v_ddi = makeSig([kWasmF64, kWasmF64, kWasmI32], []);
let kSig_ii_v = makeSig([], [kWasmI32, kWasmI32]);
let kSig_iii_v = makeSig([], [kWasmI32, kWasmI32, kWasmI32]);
let kSig_ii_i = makeSig([kWasmI32], [kWasmI32, kWasmI32]);
let kSig_iii_i = makeSig([kWasmI32], [kWasmI32, kWasmI32, kWasmI32]);
let kSig_ii_ii = makeSig([kWasmI32, kWasmI32], [kWasmI32, kWasmI32]);
let kSig_iii_ii = makeSig([kWasmI32, kWasmI32], [kWasmI32, kWasmI32, kWasmI32]);

let kSig_v_f = makeSig([kWasmF32], []);
let kSig_f_f = makeSig([kWasmF32], [kWasmF32]);
let kSig_f_d = makeSig([kWasmF64], [kWasmF32]);
let kSig_d_d = makeSig([kWasmF64], [kWasmF64]);
let kSig_r_r = makeSig([kWasmAnyRef], [kWasmAnyRef]);
let kSig_a_a = makeSig([kWasmAnyFunc], [kWasmAnyFunc]);
let kSig_e_e = makeSig([kWasmExnRef], [kWasmExnRef]);
let kSig_i_r = makeSig([kWasmAnyRef], [kWasmI32]);
let kSig_v_r = makeSig([kWasmAnyRef], []);
let kSig_v_a = makeSig([kWasmAnyFunc], []);
let kSig_v_e = makeSig([kWasmExnRef], []);
let kSig_v_rr = makeSig([kWasmAnyRef, kWasmAnyRef], []);
let kSig_v_aa = makeSig([kWasmAnyFunc, kWasmAnyFunc], []);
let kSig_r_v = makeSig([], [kWasmAnyRef]);
let kSig_a_v = makeSig([], [kWasmAnyFunc]);
let kSig_a_i = makeSig([kWasmI32], [kWasmAnyFunc]);
let kSig_e_v = makeSig([], [kWasmExnRef]);

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
let kExprUnreachable = 0x00;
let kExprNop = 0x01;
let kExprBlock = 0x02;
let kExprLoop = 0x03;
let kExprIf = 0x04;
let kExprElse = 0x05;
let kExprTry = 0x06;
let kExprCatch = 0x07;
let kExprThrow = 0x08;
let kExprRethrow = 0x09;
let kExprBrOnExn = 0x0a;
let kExprEnd = 0x0b;
let kExprBr = 0x0c;
let kExprBrIf = 0x0d;
let kExprBrTable = 0x0e;
let kExprReturn = 0x0f;
let kExprCallFunction = 0x10;
let kExprCallIndirect = 0x11;
let kExprReturnCall = 0x12;
let kExprReturnCallIndirect = 0x13;
let kExprDrop = 0x1a;
let kExprSelect = 0x1b;
let kExprGetLocal = 0x20;
let kExprSetLocal = 0x21;
let kExprTeeLocal = 0x22;
let kExprGetGlobal = 0x23;
let kExprSetGlobal = 0x24;
let kExprTableGet = 0x25;
let kExprTableSet = 0x26;
let kExprI32LoadMem = 0x28;
let kExprI64LoadMem = 0x29;
let kExprF32LoadMem = 0x2a;
let kExprF64LoadMem = 0x2b;
let kExprI32LoadMem8S = 0x2c;
let kExprI32LoadMem8U = 0x2d;
let kExprI32LoadMem16S = 0x2e;
let kExprI32LoadMem16U = 0x2f;
let kExprI64LoadMem8S = 0x30;
let kExprI64LoadMem8U = 0x31;
let kExprI64LoadMem16S = 0x32;
let kExprI64LoadMem16U = 0x33;
let kExprI64LoadMem32S = 0x34;
let kExprI64LoadMem32U = 0x35;
let kExprI32StoreMem = 0x36;
let kExprI64StoreMem = 0x37;
let kExprF32StoreMem = 0x38;
let kExprF64StoreMem = 0x39;
let kExprI32StoreMem8 = 0x3a;
let kExprI32StoreMem16 = 0x3b;
let kExprI64StoreMem8 = 0x3c;
let kExprI64StoreMem16 = 0x3d;
let kExprI64StoreMem32 = 0x3e;
let kExprMemorySize = 0x3f;
let kExprMemoryGrow = 0x40;
let kExprI32Const = 0x41;
let kExprI64Const = 0x42;
let kExprF32Const = 0x43;
let kExprF64Const = 0x44;
let kExprI32Eqz = 0x45;
let kExprI32Eq = 0x46;
let kExprI32Ne = 0x47;
let kExprI32LtS = 0x48;
let kExprI32LtU = 0x49;
let kExprI32GtS = 0x4a;
let kExprI32GtU = 0x4b;
let kExprI32LeS = 0x4c;
let kExprI32LeU = 0x4d;
let kExprI32GeS = 0x4e;
let kExprI32GeU = 0x4f;
let kExprI64Eqz = 0x50;
let kExprI64Eq = 0x51;
let kExprI64Ne = 0x52;
let kExprI64LtS = 0x53;
let kExprI64LtU = 0x54;
let kExprI64GtS = 0x55;
let kExprI64GtU = 0x56;
let kExprI64LeS = 0x57;
let kExprI64LeU = 0x58;
let kExprI64GeS = 0x59;
let kExprI64GeU = 0x5a;
let kExprF32Eq = 0x5b;
let kExprF32Ne = 0x5c;
let kExprF32Lt = 0x5d;
let kExprF32Gt = 0x5e;
let kExprF32Le = 0x5f;
let kExprF32Ge = 0x60;
let kExprF64Eq = 0x61;
let kExprF64Ne = 0x62;
let kExprF64Lt = 0x63;
let kExprF64Gt = 0x64;
let kExprF64Le = 0x65;
let kExprF64Ge = 0x66;
let kExprI32Clz = 0x67;
let kExprI32Ctz = 0x68;
let kExprI32Popcnt = 0x69;
let kExprI32Add = 0x6a;
let kExprI32Sub = 0x6b;
let kExprI32Mul = 0x6c;
let kExprI32DivS = 0x6d;
let kExprI32DivU = 0x6e;
let kExprI32RemS = 0x6f;
let kExprI32RemU = 0x70;
let kExprI32And = 0x71;
let kExprI32Ior = 0x72;
let kExprI32Xor = 0x73;
let kExprI32Shl = 0x74;
let kExprI32ShrS = 0x75;
let kExprI32ShrU = 0x76;
let kExprI32Rol = 0x77;
let kExprI32Ror = 0x78;
let kExprI64Clz = 0x79;
let kExprI64Ctz = 0x7a;
let kExprI64Popcnt = 0x7b;
let kExprI64Add = 0x7c;
let kExprI64Sub = 0x7d;
let kExprI64Mul = 0x7e;
let kExprI64DivS = 0x7f;
let kExprI64DivU = 0x80;
let kExprI64RemS = 0x81;
let kExprI64RemU = 0x82;
let kExprI64And = 0x83;
let kExprI64Ior = 0x84;
let kExprI64Xor = 0x85;
let kExprI64Shl = 0x86;
let kExprI64ShrS = 0x87;
let kExprI64ShrU = 0x88;
let kExprI64Rol = 0x89;
let kExprI64Ror = 0x8a;
let kExprF32Abs = 0x8b;
let kExprF32Neg = 0x8c;
let kExprF32Ceil = 0x8d;
let kExprF32Floor = 0x8e;
let kExprF32Trunc = 0x8f;
let kExprF32NearestInt = 0x90;
let kExprF32Sqrt = 0x91;
let kExprF32Add = 0x92;
let kExprF32Sub = 0x93;
let kExprF32Mul = 0x94;
let kExprF32Div = 0x95;
let kExprF32Min = 0x96;
let kExprF32Max = 0x97;
let kExprF32CopySign = 0x98;
let kExprF64Abs = 0x99;
let kExprF64Neg = 0x9a;
let kExprF64Ceil = 0x9b;
let kExprF64Floor = 0x9c;
let kExprF64Trunc = 0x9d;
let kExprF64NearestInt = 0x9e;
let kExprF64Sqrt = 0x9f;
let kExprF64Add = 0xa0;
let kExprF64Sub = 0xa1;
let kExprF64Mul = 0xa2;
let kExprF64Div = 0xa3;
let kExprF64Min = 0xa4;
let kExprF64Max = 0xa5;
let kExprF64CopySign = 0xa6;
let kExprI32ConvertI64 = 0xa7;
let kExprI32SConvertF32 = 0xa8;
let kExprI32UConvertF32 = 0xa9;
let kExprI32SConvertF64 = 0xaa;
let kExprI32UConvertF64 = 0xab;
let kExprI64SConvertI32 = 0xac;
let kExprI64UConvertI32 = 0xad;
let kExprI64SConvertF32 = 0xae;
let kExprI64UConvertF32 = 0xaf;
let kExprI64SConvertF64 = 0xb0;
let kExprI64UConvertF64 = 0xb1;
let kExprF32SConvertI32 = 0xb2;
let kExprF32UConvertI32 = 0xb3;
let kExprF32SConvertI64 = 0xb4;
let kExprF32UConvertI64 = 0xb5;
let kExprF32ConvertF64 = 0xb6;
let kExprF64SConvertI32 = 0xb7;
let kExprF64UConvertI32 = 0xb8;
let kExprF64SConvertI64 = 0xb9;
let kExprF64UConvertI64 = 0xba;
let kExprF64ConvertF32 = 0xbb;
let kExprI32ReinterpretF32 = 0xbc;
let kExprI64ReinterpretF64 = 0xbd;
let kExprF32ReinterpretI32 = 0xbe;
let kExprF64ReinterpretI64 = 0xbf;
let kExprI32SExtendI8 = 0xc0;
let kExprI32SExtendI16 = 0xc1;
let kExprI64SExtendI8 = 0xc2;
let kExprI64SExtendI16 = 0xc3;
let kExprI64SExtendI32 = 0xc4;
let kExprRefNull = 0xd0;
let kExprRefIsNull = 0xd1;
let kExprRefFunc = 0xd2;

// Prefix opcodes
let kNumericPrefix = 0xfc;
let kSimdPrefix = 0xfd;
let kAtomicPrefix = 0xfe;

// Numeric opcodes.
let kExprMemoryInit = 0x08;
let kExprDataDrop = 0x09;
let kExprMemoryCopy = 0x0a;
let kExprMemoryFill = 0x0b;
let kExprTableInit = 0x0c;
let kExprElemDrop = 0x0d;
let kExprTableCopy = 0x0e;
let kExprTableGrow = 0x0f;
let kExprTableSize = 0x10;
let kExprTableFill = 0x11;

// Atomic opcodes.
let kExprAtomicNotify = 0x00;
let kExprI32AtomicWait = 0x01;
let kExprI64AtomicWait = 0x02;
let kExprI32AtomicLoad = 0x10;
let kExprI32AtomicLoad8U = 0x12;
let kExprI32AtomicLoad16U = 0x13;
let kExprI32AtomicStore = 0x17;
let kExprI32AtomicStore8U = 0x19;
let kExprI32AtomicStore16U = 0x1a;
let kExprI32AtomicAdd = 0x1e;
let kExprI32AtomicAdd8U = 0x20;
let kExprI32AtomicAdd16U = 0x21;
let kExprI32AtomicSub = 0x25;
let kExprI32AtomicSub8U = 0x27;
let kExprI32AtomicSub16U = 0x28;
let kExprI32AtomicAnd = 0x2c;
let kExprI32AtomicAnd8U = 0x2e;
let kExprI32AtomicAnd16U = 0x2f;
let kExprI32AtomicOr = 0x33;
let kExprI32AtomicOr8U = 0x35;
let kExprI32AtomicOr16U = 0x36;
let kExprI32AtomicXor = 0x3a;
let kExprI32AtomicXor8U = 0x3c;
let kExprI32AtomicXor16U = 0x3d;
let kExprI32AtomicExchange = 0x41;
let kExprI32AtomicExchange8U = 0x43;
let kExprI32AtomicExchange16U = 0x44;
let kExprI32AtomicCompareExchange = 0x48;
let kExprI32AtomicCompareExchange8U = 0x4a;
let kExprI32AtomicCompareExchange16U = 0x4b;

let kExprI64AtomicLoad = 0x11;
let kExprI64AtomicLoad8U = 0x14;
let kExprI64AtomicLoad16U = 0x15;
let kExprI64AtomicLoad32U = 0x16;
let kExprI64AtomicStore = 0x18;
let kExprI64AtomicStore8U = 0x1b;
let kExprI64AtomicStore16U = 0x1c;
let kExprI64AtomicStore32U = 0x1d;
let kExprI64AtomicAdd = 0x1f;
let kExprI64AtomicAdd8U = 0x22;
let kExprI64AtomicAdd16U = 0x23;
let kExprI64AtomicAdd32U = 0x24;
let kExprI64AtomicSub = 0x26;
let kExprI64AtomicSub8U = 0x29;
let kExprI64AtomicSub16U = 0x2a;
let kExprI64AtomicSub32U = 0x2b;
let kExprI64AtomicAnd = 0x2d;
let kExprI64AtomicAnd8U = 0x30;
let kExprI64AtomicAnd16U = 0x31;
let kExprI64AtomicAnd32U = 0x32;
let kExprI64AtomicOr = 0x34;
let kExprI64AtomicOr8U = 0x37;
let kExprI64AtomicOr16U = 0x38;
let kExprI64AtomicOr32U = 0x39;
let kExprI64AtomicXor = 0x3b;
let kExprI64AtomicXor8U = 0x3e;
let kExprI64AtomicXor16U = 0x3f;
let kExprI64AtomicXor32U = 0x40;
let kExprI64AtomicExchange = 0x42;
let kExprI64AtomicExchange8U = 0x45;
let kExprI64AtomicExchange16U = 0x46;
let kExprI64AtomicExchange32U = 0x47;
let kExprI64AtomicCompareExchange = 0x49
let kExprI64AtomicCompareExchange8U = 0x4c;
let kExprI64AtomicCompareExchange16U = 0x4d;
let kExprI64AtomicCompareExchange32U = 0x4e;

// Simd opcodes.
let kExprF32x4Min = 0x9e;

// Compilation hint constants.
let kCompilationHintStrategyDefault = 0x00;
let kCompilationHintStrategyLazy = 0x01;
let kCompilationHintStrategyEager = 0x02;
let kCompilationHintStrategyLazyBaselineEagerTopTier = 0x03;
let kCompilationHintTierDefault = 0x00;
let kCompilationHintTierInterpreter = 0x01;
let kCompilationHintTierBaseline = 0x02;
let kCompilationHintTierOptimized = 0x03;

let kTrapUnreachable          = 0;
let kTrapMemOutOfBounds       = 1;
let kTrapDivByZero            = 2;
let kTrapDivUnrepresentable   = 3;
let kTrapRemByZero            = 4;
let kTrapFloatUnrepresentable = 5;
let kTrapFuncInvalid          = 6;
let kTrapFuncSigMismatch      = 7;
let kTrapTypeError            = 8;
let kTrapUnalignedAccess      = 9;
let kTrapDataSegmentDropped   = 10;
let kTrapElemSegmentDropped   = 11;
let kTrapTableOutOfBounds     = 12;

let kTrapMsgs = [
  "unreachable",
  "memory access out of bounds",
  "divide by zero",
  "divide result unrepresentable",
  "remainder by zero",
  "float unrepresentable in integer range",
  "invalid index into function table",
  "function signature mismatch",
  "wasm function signature contains illegal type",
  "operation does not support unaligned accesses",
  "data segment has been dropped",
  "element segment has been dropped",
  "table access out of bounds"
];

function assertTraps(trap, code) {
  assertThrows(code, WebAssembly.RuntimeError, kTrapMsgs[trap]);
}

class Binary {
  constructor() {
    this.length = 0;
    this.buffer = new Uint8Array(8192);
  }

  ensure_space(needed) {
    if (this.buffer.length - this.length >= needed) return;
    let new_capacity = this.buffer.length * 2;
    while (new_capacity - this.length < needed) new_capacity *= 2;
    let new_buffer = new Uint8Array(new_capacity);
    new_buffer.set(this.buffer);
    this.buffer = new_buffer;
  }

  trunc_buffer() {
    return new Uint8Array(this.buffer.buffer, 0, this.length);
  }

  reset() {
    this.length = 0;
  }

  emit_u8(val) {
    this.ensure_space(1);
    this.buffer[this.length++] = val;
  }

  emit_u16(val) {
    this.ensure_space(2);
    this.buffer[this.length++] = val;
    this.buffer[this.length++] = val >> 8;
  }

  emit_u32(val) {
    this.ensure_space(4);
    this.buffer[this.length++] = val;
    this.buffer[this.length++] = val >> 8;
    this.buffer[this.length++] = val >> 16;
    this.buffer[this.length++] = val >> 24;
  }

  emit_leb_u(val, max_len) {
    this.ensure_space(max_len);
    for (let i = 0; i < max_len; ++i) {
      let v = val & 0xff;
      val = val >>> 7;
      if (val == 0) {
        this.buffer[this.length++] = v;
        return;
      }
      this.buffer[this.length++] = v | 0x80;
    }
    throw new Error("Leb value exceeds maximum length of " + max_len);
  }

  emit_u32v(val) {
    this.emit_leb_u(val, kMaxVarInt32Size);
  }

  emit_u64v(val) {
    this.emit_leb_u(val, kMaxVarInt64Size);
  }

  emit_bytes(data) {
    this.ensure_space(data.length);
    this.buffer.set(data, this.length);
    this.length += data.length;
  }

  emit_string(string) {
    // When testing illegal names, we pass a byte array directly.
    if (string instanceof Array) {
      this.emit_u32v(string.length);
      this.emit_bytes(string);
      return;
    }

    // This is the hacky way to convert a JavaScript string to a UTF8 encoded
    // string only containing single-byte characters.
    let string_utf8 = unescape(encodeURIComponent(string));
    this.emit_u32v(string_utf8.length);
    for (let i = 0; i < string_utf8.length; i++) {
      this.emit_u8(string_utf8.charCodeAt(i));
    }
  }

  emit_header() {
    this.emit_bytes([
      kWasmH0, kWasmH1, kWasmH2, kWasmH3, kWasmV0, kWasmV1, kWasmV2, kWasmV3
    ]);
  }

  emit_section(section_code, content_generator) {
    // Emit section name.
    this.emit_u8(section_code);
    // Emit the section to a temporary buffer: its full length isn't know yet.
    const section = new Binary;
    content_generator(section);
    // Emit section length.
    this.emit_u32v(section.length);
    // Copy the temporary buffer.
    // Avoid spread because {section} can be huge.
    this.emit_bytes(section.trunc_buffer());
  }
}

class WasmFunctionBuilder {
  constructor(module, name, type_index) {
    this.module = module;
    this.name = name;
    this.type_index = type_index;
    this.body = [];
    this.locals = [];
    this.local_names = [];
  }

  numLocalNames() {
    let num_local_names = 0;
    for (let loc_name of this.local_names) {
      if (loc_name !== undefined) ++num_local_names;
    }
    return num_local_names;
  }

  exportAs(name) {
    this.module.addExport(name, this.index);
    return this;
  }

  exportFunc() {
    this.exportAs(this.name);
    return this;
  }

  setCompilationHint(strategy, baselineTier, topTier) {
    this.module.setCompilationHint(strategy, baselineTier, topTier, this.index);
    return this;
  }

  addBody(body) {
    for (let b of body) {
      if (typeof b !== 'number' || (b & (~0xFF)) !== 0 )
        throw new Error('invalid body (entries must be 8 bit numbers): ' + body);
    }
    this.body = body.slice();
    // Automatically add the end for the function block to the body.
    this.body.push(kExprEnd);
    return this;
  }

  addBodyWithEnd(body) {
    this.body = body;
    return this;
  }

  getNumLocals() {
    let total_locals = 0;
    for (let l of this.locals) {
      for (let type of ["i32", "i64", "f32", "f64", "s128"]) {
        total_locals += l[type + "_count"] || 0;
      }
    }
    return total_locals;
  }

  addLocals(locals, names) {
    const old_num_locals = this.getNumLocals();
    this.locals.push(locals);
    if (names) {
      const missing_names = old_num_locals - this.local_names.length;
      this.local_names.push(...new Array(missing_names), ...names);
    }
    return this;
  }

  end() {
    return this.module;
  }
}

class WasmGlobalBuilder {
  constructor(module, type, mutable) {
    this.module = module;
    this.type = type;
    this.mutable = mutable;
    this.init = 0;
  }

  exportAs(name) {
    this.module.exports.push({name: name, kind: kExternalGlobal,
                              index: this.index});
    return this;
  }
}

class WasmTableBuilder {
  constructor(module, type, initial_size, max_size) {
    this.module = module;
    this.type = type;
    this.initial_size = initial_size;
    this.has_max = max_size != undefined;
    this.max_size = max_size;
  }

  exportAs(name) {
    this.module.exports.push({name: name, kind: kExternalTable,
                              index: this.index});
    return this;
  }
}

class WasmModuleBuilder {
  constructor() {
    this.types = [];
    this.imports = [];
    this.exports = [];
    this.globals = [];
    this.tables = [];
    this.exceptions = [];
    this.functions = [];
    this.compilation_hints = [];
    this.element_segments = [];
    this.data_segments = [];
    this.explicit = [];
    this.num_imported_funcs = 0;
    this.num_imported_globals = 0;
    this.num_imported_tables = 0;
    this.num_imported_exceptions = 0;
    return this;
  }

  addStart(start_index) {
    this.start_index = start_index;
    return this;
  }

  addMemory(min, max, exp, shared) {
    this.memory = {min: min, max: max, exp: exp, shared: shared};
    return this;
  }

  addExplicitSection(bytes) {
    this.explicit.push(bytes);
    return this;
  }

  stringToBytes(name) {
    var result = new Binary();
    result.emit_u32v(name.length);
    for (var i = 0; i < name.length; i++) {
      result.emit_u8(name.charCodeAt(i));
    }
    return result.trunc_buffer()
  }

  createCustomSection(name, bytes) {
    name = this.stringToBytes(name);
    var section = new Binary();
    section.emit_u8(0);
    section.emit_u32v(name.length + bytes.length);
    section.emit_bytes(name);
    section.emit_bytes(bytes);
    return section.trunc_buffer();
  }

  addCustomSection(name, bytes) {
    this.explicit.push(this.createCustomSection(name, bytes));
  }

  addType(type) {
    this.types.push(type);
    var pl = type.params.length;  // should have params
    var rl = type.results.length; // should have results
    return this.types.length - 1;
  }

  addGlobal(local_type, mutable) {
    let glob = new WasmGlobalBuilder(this, local_type, mutable);
    glob.index = this.globals.length + this.num_imported_globals;
    this.globals.push(glob);
    return glob;
  }

  addTable(type, initial_size, max_size = undefined) {
    if (type != kWasmAnyRef && type != kWasmAnyFunc) {
      throw new Error('Tables must be of type kWasmAnyRef or kWasmAnyFunc');
    }
    let table = new WasmTableBuilder(this, type, initial_size, max_size);
    table.index = this.tables.length + this.num_imported_tables;
    this.tables.push(table);
    return table;
  }

  addException(type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let except_index = this.exceptions.length + this.num_imported_exceptions;
    this.exceptions.push(type_index);
    return except_index;
  }

  addFunction(name, type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let func = new WasmFunctionBuilder(this, name, type_index);
    func.index = this.functions.length + this.num_imported_funcs;
    this.functions.push(func);
    return func;
  }

  addImport(module, name, type) {
    if (this.functions.length != 0) {
      throw new Error('Imported functions must be declared before local ones');
    }
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    this.imports.push({module: module, name: name, kind: kExternalFunction,
                       type: type_index});
    return this.num_imported_funcs++;
  }

  addImportedGlobal(module, name, type, mutable = false) {
    if (this.globals.length != 0) {
      throw new Error('Imported globals must be declared before local ones');
    }
    let o = {module: module, name: name, kind: kExternalGlobal, type: type,
             mutable: mutable};
    this.imports.push(o);
    return this.num_imported_globals++;
  }

  addImportedMemory(module, name, initial = 0, maximum, shared) {
    let o = {module: module, name: name, kind: kExternalMemory,
             initial: initial, maximum: maximum, shared: shared};
    this.imports.push(o);
    return this;
  }

  addImportedTable(module, name, initial, maximum, type) {
    if (this.tables.length != 0) {
      throw new Error('Imported tables must be declared before local ones');
    }
    let o = {module: module, name: name, kind: kExternalTable, initial: initial,
             maximum: maximum, type: type || kWasmAnyFunctionTypeForm};
    this.imports.push(o);
    return this.num_imported_tables++;
  }

  addImportedException(module, name, type) {
    if (this.exceptions.length != 0) {
      throw new Error('Imported exceptions must be declared before local ones');
    }
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let o = {module: module, name: name, kind: kExternalException, type: type_index};
    this.imports.push(o);
    return this.num_imported_exceptions++;
  }

  addExport(name, index) {
    this.exports.push({name: name, kind: kExternalFunction, index: index});
    return this;
  }

  addExportOfKind(name, kind, index) {
    this.exports.push({name: name, kind: kind, index: index});
    return this;
  }

  setCompilationHint(strategy, baselineTier, topTier, index) {
    this.compilation_hints[index] = {strategy: strategy, baselineTier:
      baselineTier, topTier: topTier};
    return this;
  }

  addDataSegment(addr, data, is_global = false) {
    this.data_segments.push(
        {addr: addr, data: data, is_global: is_global, is_active: true});
    return this.data_segments.length - 1;
  }

  addPassiveDataSegment(data) {
    this.data_segments.push({data: data, is_active: false});
    return this.data_segments.length - 1;
  }

  exportMemoryAs(name) {
    this.exports.push({name: name, kind: kExternalMemory, index: 0});
  }

  addElementSegment(table, base, is_global, array) {
    this.element_segments.push({table: table, base: base, is_global: is_global,
                                    array: array, is_active: true});
    return this;
  }

  addPassiveElementSegment(array, is_import = false) {
    this.element_segments.push({array: array, is_active: false});
    return this;
  }

  appendToTable(array) {
    for (let n of array) {
      if (typeof n != 'number')
        throw new Error('invalid table (entries have to be numbers): ' + array);
    }
    if (this.tables.length == 0) {
      this.addTable(kWasmAnyFunc, 0);
    }
    // Adjust the table to the correct size.
    let table = this.tables[0];
    const base = table.initial_size;
    const table_size = base + array.length;
    table.initial_size = table_size;
    if (table.has_max && table_size > table.max_size) {
      table.max_size = table_size;
    }
    return this.addElementSegment(0, base, false, array);
  }

  setTableBounds(min, max = undefined) {
    if (this.tables.length != 0) {
      throw new Error("The table bounds of table '0' have already been set.");
    }
    this.addTable(kWasmAnyFunc, min, max);
    return this;
  }

  setName(name) {
    this.name = name;
    return this;
  }

  toBuffer(debug = false) {
    let binary = new Binary;
    let wasm = this;

    // Add header
    binary.emit_header();

    // Add type section
    if (wasm.types.length > 0) {
      if (debug) print("emitting types @ " + binary.length);
      binary.emit_section(kTypeSectionCode, section => {
        section.emit_u32v(wasm.types.length);
        for (let type of wasm.types) {
          section.emit_u8(kWasmFunctionTypeForm);
          section.emit_u32v(type.params.length);
          for (let param of type.params) {
            section.emit_u8(param);
          }
          section.emit_u32v(type.results.length);
          for (let result of type.results) {
            section.emit_u8(result);
          }
        }
      });
    }

    // Add imports section
    if (wasm.imports.length > 0) {
      if (debug) print("emitting imports @ " + binary.length);
      binary.emit_section(kImportSectionCode, section => {
        section.emit_u32v(wasm.imports.length);
        for (let imp of wasm.imports) {
          section.emit_string(imp.module);
          section.emit_string(imp.name || '');
          section.emit_u8(imp.kind);
          if (imp.kind == kExternalFunction) {
            section.emit_u32v(imp.type);
          } else if (imp.kind == kExternalGlobal) {
            section.emit_u32v(imp.type);
            section.emit_u8(imp.mutable);
          } else if (imp.kind == kExternalMemory) {
            var has_max = (typeof imp.maximum) != "undefined";
            var is_shared = (typeof imp.shared) != "undefined";
            if (is_shared) {
              section.emit_u8(has_max ? 3 : 2); // flags
            } else {
              section.emit_u8(has_max ? 1 : 0); // flags
            }
            section.emit_u32v(imp.initial); // initial
            if (has_max) section.emit_u32v(imp.maximum); // maximum
          } else if (imp.kind == kExternalTable) {
            section.emit_u8(imp.type);
            var has_max = (typeof imp.maximum) != "undefined";
            section.emit_u8(has_max ? 1 : 0); // flags
            section.emit_u32v(imp.initial); // initial
            if (has_max) section.emit_u32v(imp.maximum); // maximum
          } else if (imp.kind == kExternalException) {
            section.emit_u32v(kExceptionAttribute);
            section.emit_u32v(imp.type);
          } else {
            throw new Error("unknown/unsupported import kind " + imp.kind);
          }
        }
      });
    }

    // Add functions declarations
    if (wasm.functions.length > 0) {
      if (debug) print("emitting function decls @ " + binary.length);
      binary.emit_section(kFunctionSectionCode, section => {
        section.emit_u32v(wasm.functions.length);
        for (let func of wasm.functions) {
          section.emit_u32v(func.type_index);
        }
      });
    }

    // Add table section
    if (wasm.tables.length > 0) {
      if (debug) print ("emitting tables @ " + binary.length);
      binary.emit_section(kTableSectionCode, section => {
        section.emit_u32v(wasm.tables.length);
        for (let table of wasm.tables) {
          section.emit_u8(table.type);
          section.emit_u8(table.has_max);
          section.emit_u32v(table.initial_size);
          if (table.has_max) section.emit_u32v(table.max_size);
        }
      });
    }

    // Add memory section
    if (wasm.memory !== undefined) {
      if (debug) print("emitting memory @ " + binary.length);
      binary.emit_section(kMemorySectionCode, section => {
        section.emit_u8(1);  // one memory entry
        const has_max = wasm.memory.max !== undefined;
        const is_shared = wasm.memory.shared !== undefined;
        // Emit flags (bit 0: reszeable max, bit 1: shared memory)
        if (is_shared) {
          section.emit_u8(has_max ? kSharedHasMaximumFlag : 2);
        } else {
          section.emit_u8(has_max ? kHasMaximumFlag : 0);
        }
        section.emit_u32v(wasm.memory.min);
        if (has_max) section.emit_u32v(wasm.memory.max);
      });
    }

    // Add global section.
    if (wasm.globals.length > 0) {
      if (debug) print ("emitting globals @ " + binary.length);
      binary.emit_section(kGlobalSectionCode, section => {
        section.emit_u32v(wasm.globals.length);
        for (let global of wasm.globals) {
          section.emit_u8(global.type);
          section.emit_u8(global.mutable);
          if ((typeof global.init_index) == "undefined") {
            // Emit a constant initializer.
            switch (global.type) {
            case kWasmI32:
              section.emit_u8(kExprI32Const);
              section.emit_u32v(global.init);
              break;
            case kWasmI64:
              section.emit_u8(kExprI64Const);
              section.emit_u64v(global.init);
              break;
            case kWasmF32:
              section.emit_u8(kExprF32Const);
              f32_view[0] = global.init;
              section.emit_bytes(f32_bytes_view);
              break;
            case kWasmF64:
              section.emit_u8(kExprF64Const);
              f64_view[0] = global.init;
              section.emit_bytes(f64_bytes_view);
              break;
            case kWasmAnyFunc:
            case kWasmAnyRef:
              if (global.function_index !== undefined) {
                section.emit_u8(kExprRefFunc);
                section.emit_u32v(global.function_index);
              } else {
                section.emit_u8(kExprRefNull);
              }
              break;
            case kWasmExnRef:
              section.emit_u8(kExprRefNull);
              break;
            }
          } else {
            // Emit a global-index initializer.
            section.emit_u8(kExprGetGlobal);
            section.emit_u32v(global.init_index);
          }
          section.emit_u8(kExprEnd);  // end of init expression
        }
      });
    }

    // Add exceptions.
    if (wasm.exceptions.length > 0) {
      if (debug) print("emitting exceptions @ " + binary.length);
      binary.emit_section(kExceptionSectionCode, section => {
        section.emit_u32v(wasm.exceptions.length);
        for (let type of wasm.exceptions) {
          section.emit_u32v(kExceptionAttribute);
          section.emit_u32v(type);
        }
      });
    }

    // Add export table.
    var mem_export = (wasm.memory !== undefined && wasm.memory.exp);
    var exports_count = wasm.exports.length + (mem_export ? 1 : 0);
    if (exports_count > 0) {
      if (debug) print("emitting exports @ " + binary.length);
      binary.emit_section(kExportSectionCode, section => {
        section.emit_u32v(exports_count);
        for (let exp of wasm.exports) {
          section.emit_string(exp.name);
          section.emit_u8(exp.kind);
          section.emit_u32v(exp.index);
        }
        if (mem_export) {
          section.emit_string("memory");
          section.emit_u8(kExternalMemory);
          section.emit_u8(0);
        }
      });
    }

    // Add start function section.
    if (wasm.start_index !== undefined) {
      if (debug) print("emitting start function @ " + binary.length);
      binary.emit_section(kStartSectionCode, section => {
        section.emit_u32v(wasm.start_index);
      });
    }

    // Add element segments
    if (wasm.element_segments.length > 0) {
      if (debug) print("emitting element segments @ " + binary.length);
      binary.emit_section(kElementSectionCode, section => {
        var inits = wasm.element_segments;
        section.emit_u32v(inits.length);

        for (let init of inits) {
          if (init.is_active) {
            // Active segment.
            if (init.table == 0) {
              section.emit_u32v(kActiveNoIndex);
            } else {
              section.emit_u32v(kActiveWithIndex);
              section.emit_u32v(init.table);
            }
            if (init.is_global) {
              section.emit_u8(kExprGetGlobal);
            } else {
              section.emit_u8(kExprI32Const);
            }
            section.emit_u32v(init.base);
            section.emit_u8(kExprEnd);
            section.emit_u32v(init.array.length);
            for (let index of init.array) {
              section.emit_u32v(index);
            }
          } else {
            // Passive segment.
            section.emit_u8(kPassive);  // flags
            section.emit_u8(kWasmAnyFunc);
            section.emit_u32v(init.array.length);
            for (let index of init.array) {
              if (index === null) {
                section.emit_u8(kExprRefNull);
                section.emit_u8(kExprEnd);
              } else {
                section.emit_u8(kExprRefFunc);
                section.emit_u32v(index);
                section.emit_u8(kExprEnd);
              }
            }
          }
        }
      });
    }

    // If there are any passive data segments, add the DataCount section.
    if (wasm.data_segments.some(seg => !seg.is_active)) {
      binary.emit_section(kDataCountSectionCode, section => {
        section.emit_u32v(wasm.data_segments.length);
      });
    }

    // If there are compilation hints add a custom section 'compilationHints'
    // after the function section and before the code section.
    if (wasm.compilation_hints.length > 0) {
      if (debug) print("emitting compilation hints @ " + binary.length);
      // Build custom section payload.
      let payloadBinary = new Binary();
      let implicit_compilation_hints_count = wasm.functions.length;
      payloadBinary.emit_u32v(implicit_compilation_hints_count);

      // Defaults to the compiler's choice if no better hint was given (0x00).
      let defaultHintByte = kCompilationHintStrategyDefault |
          (kCompilationHintTierDefault << 2) |
          (kCompilationHintTierDefault << 4);

      // Emit hint byte for every function defined in this module.
      for (let i = 0; i < implicit_compilation_hints_count; i++) {
        let index = wasm.num_imported_funcs + i;
        var hintByte;
        if(index in wasm.compilation_hints) {
          let hint = wasm.compilation_hints[index];
          hintByte = hint.strategy | (hint.baselineTier << 2) |
              (hint.topTier << 4);
        } else{
          hintByte = defaultHintByte;
        }
        payloadBinary.emit_u8(hintByte);
      }

      // Finalize as custom section.
      let name = "compilationHints";
      let bytes = this.createCustomSection(name, payloadBinary.trunc_buffer());
      binary.emit_bytes(bytes);
    }

    // Add function bodies.
    if (wasm.functions.length > 0) {
      // emit function bodies
      if (debug) print("emitting code @ " + binary.length);
      binary.emit_section(kCodeSectionCode, section => {
        section.emit_u32v(wasm.functions.length);
        let header = new Binary;
        for (let func of wasm.functions) {
          header.reset();
          // Function body length will be patched later.
          let local_decls = [];
          for (let l of func.locals || []) {
            if (l.i32_count > 0) {
              local_decls.push({count: l.i32_count, type: kWasmI32});
            }
            if (l.i64_count > 0) {
              local_decls.push({count: l.i64_count, type: kWasmI64});
            }
            if (l.f32_count > 0) {
              local_decls.push({count: l.f32_count, type: kWasmF32});
            }
            if (l.f64_count > 0) {
              local_decls.push({count: l.f64_count, type: kWasmF64});
            }
            if (l.s128_count > 0) {
              local_decls.push({count: l.s128_count, type: kWasmS128});
            }
            if (l.anyref_count > 0) {
              local_decls.push({count: l.anyref_count, type: kWasmAnyRef});
            }
            if (l.anyfunc_count > 0) {
              local_decls.push({count: l.anyfunc_count, type: kWasmAnyFunc});
            }
            if (l.except_count > 0) {
              local_decls.push({count: l.except_count, type: kWasmExnRef});
            }
          }

          header.emit_u32v(local_decls.length);
          for (let decl of local_decls) {
            header.emit_u32v(decl.count);
            header.emit_u8(decl.type);
          }

          section.emit_u32v(header.length + func.body.length);
          section.emit_bytes(header.trunc_buffer());
          section.emit_bytes(func.body);
        }
      });
    }

    // Add data segments.
    if (wasm.data_segments.length > 0) {
      if (debug) print("emitting data segments @ " + binary.length);
      binary.emit_section(kDataSectionCode, section => {
        section.emit_u32v(wasm.data_segments.length);
        for (let seg of wasm.data_segments) {
          if (seg.is_active) {
            section.emit_u8(0);  // linear memory index 0 / flags
            if (seg.is_global) {
              // initializer is a global variable
              section.emit_u8(kExprGetGlobal);
              section.emit_u32v(seg.addr);
            } else {
              // initializer is a constant
              section.emit_u8(kExprI32Const);
              section.emit_u32v(seg.addr);
            }
            section.emit_u8(kExprEnd);
          } else {
            section.emit_u8(kPassive);  // flags
          }
          section.emit_u32v(seg.data.length);
          section.emit_bytes(seg.data);
        }
      });
    }

    // Add any explicitly added sections
    for (let exp of wasm.explicit) {
      if (debug) print("emitting explicit @ " + binary.length);
      binary.emit_bytes(exp);
    }

    // Add names.
    let num_function_names = 0;
    let num_functions_with_local_names = 0;
    for (let func of wasm.functions) {
      if (func.name !== undefined) ++num_function_names;
      if (func.numLocalNames() > 0) ++num_functions_with_local_names;
    }
    if (num_function_names > 0 || num_functions_with_local_names > 0 ||
        wasm.name !== undefined) {
      if (debug) print('emitting names @ ' + binary.length);
      binary.emit_section(kUnknownSectionCode, section => {
        section.emit_string('name');
        // Emit module name.
        if (wasm.name !== undefined) {
          section.emit_section(kModuleNameCode, name_section => {
            name_section.emit_string(wasm.name);
          });
        }
        // Emit function names.
        if (num_function_names > 0) {
          section.emit_section(kFunctionNamesCode, name_section => {
            name_section.emit_u32v(num_function_names);
            for (let func of wasm.functions) {
              if (func.name === undefined) continue;
              name_section.emit_u32v(func.index);
              name_section.emit_string(func.name);
            }
          });
        }
        // Emit local names.
        if (num_functions_with_local_names > 0) {
          section.emit_section(kLocalNamesCode, name_section => {
            name_section.emit_u32v(num_functions_with_local_names);
            for (let func of wasm.functions) {
              if (func.numLocalNames() == 0) continue;
              name_section.emit_u32v(func.index);
              name_section.emit_u32v(func.numLocalNames());
              for (let i = 0; i < func.local_names.length; ++i) {
                if (func.local_names[i] === undefined) continue;
                name_section.emit_u32v(i);
                name_section.emit_string(func.local_names[i]);
              }
            }
          });
        }
      });
    }

    return binary.trunc_buffer();
  }

  toArray(debug = false) {
    return Array.from(this.toBuffer(debug));
  }

  instantiate(ffi) {
    let module = this.toModule();
    let instance = new WebAssembly.Instance(module, ffi);
    return instance;
  }

  asyncInstantiate(ffi) {
    return WebAssembly.instantiate(this.toBuffer(), ffi)
        .then(({module, instance}) => instance);
  }

  toModule(debug = false) {
    return new WebAssembly.Module(this.toBuffer(debug));
  }
}

function wasmSignedLeb(val, max_len = 5) {
  let res = [];
  for (let i = 0; i < max_len; ++i) {
    let v = val & 0x7f;
    // If {v} sign-extended from 7 to 32 bits is equal to val, we are done.
    if (((v << 25) >> 25) == val) {
      res.push(v);
      return res;
    }
    res.push(v | 0x80);
    val = val >> 7;
  }
  throw new Error(
      'Leb value <' + val + '> exceeds maximum length of ' + max_len);
}

function wasmI32Const(val) {
  return [kExprI32Const, ...wasmSignedLeb(val, 5)];
}

function wasmF32Const(f) {
  f32_view[0] = f;
  return [
    kExprF32Const, f32_bytes_view[0], f32_bytes_view[1], f32_bytes_view[2],
    f32_bytes_view[3]
  ];
}

function wasmF64Const(f) {
  f64_view[0] = f;
  return [
    kExprF64Const, f64_bytes_view[0], f64_bytes_view[1], f64_bytes_view[2],
    f64_bytes_view[3], f64_bytes_view[4], f64_bytes_view[5], f64_bytes_view[6],
    f64_bytes_view[7]
  ];
}
