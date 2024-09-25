// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for encoding f32 and double constants to bits.
let byte_view = new Uint8Array(8);
let data_view = new DataView(byte_view.buffer);

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
let kTagSectionCode = 13;        // Tag section (between Memory & Global)

// Name section types
let kModuleNameCode = 0;
let kFunctionNamesCode = 1;
let kLocalNamesCode = 2;

let kWasmFunctionTypeForm = 0x60;
let kWasmAnyFunctionTypeForm = 0x70;
let kWasmStructTypeForm = 0x5f;
let kWasmArrayTypeForm = 0x5e;
let kWasmSubtypeForm = 0x50;
let kWasmSubtypeFinalForm = 0x4f;
let kWasmRecursiveTypeGroupForm = 0x4e;

let kNoSuperType = 0xFFFFFFFF;

let kHasMaximumFlag = 1;
let kSharedHasMaximumFlag = 3;

// Segment flags
let kActiveNoIndex = 0;
let kPassive = 1;
let kActiveWithIndex = 2;
let kPassiveWithElements = 5;

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

// Packed storage types
let kWasmI8 = 0x78;
let kWasmI16 = 0x77;

// These are defined as negative integers to distinguish them from positive type
// indices.
let kWasmNullFuncRef = -0x0d;
let kWasmNullExternRef = -0x0e;
let kWasmNullRef = -0x0f;
let kWasmFuncRef = -0x10;
let kWasmAnyFunc = kWasmFuncRef;  // Alias named as in the JS API spec
let kWasmExternRef = -0x11;
let kWasmAnyRef = -0x12;
let kWasmEqRef = -0x13;
let kWasmI31Ref = -0x14;
let kWasmStructRef = -0x15;
let kWasmArrayRef = -0x16;

// Use the positive-byte versions inside function bodies.
let kLeb128Mask = 0x7f;
let kFuncRefCode = kWasmFuncRef & kLeb128Mask;
let kAnyFuncCode = kFuncRefCode;  // Alias named as in the JS API spec
let kExternRefCode = kWasmExternRef & kLeb128Mask;
let kAnyRefCode = kWasmAnyRef & kLeb128Mask;
let kEqRefCode = kWasmEqRef & kLeb128Mask;
let kI31RefCode = kWasmI31Ref & kLeb128Mask;
let kNullExternRefCode = kWasmNullExternRef & kLeb128Mask;
let kNullFuncRefCode = kWasmNullFuncRef & kLeb128Mask;
let kStructRefCode = kWasmStructRef & kLeb128Mask;
let kArrayRefCode = kWasmArrayRef & kLeb128Mask;
let kNullRefCode = kWasmNullRef & kLeb128Mask;

let kWasmRefNull = 0x63;
let kWasmRef = 0x64;
function wasmRefNullType(heap_type) {
  return {opcode: kWasmRefNull, heap_type: heap_type};
}
function wasmRefType(heap_type) {
  return {opcode: kWasmRef, heap_type: heap_type};
}

let kExternalFunction = 0;
let kExternalTable = 1;
let kExternalMemory = 2;
let kExternalGlobal = 3;
let kExternalTag = 4;

let kTableZero = 0;
let kMemoryZero = 0;
let kSegmentZero = 0;

let kTagAttribute = 0;

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
let kSig_r_r = makeSig([kWasmExternRef], [kWasmExternRef]);
let kSig_a_a = makeSig([kWasmAnyFunc], [kWasmAnyFunc]);
let kSig_i_r = makeSig([kWasmExternRef], [kWasmI32]);
let kSig_v_r = makeSig([kWasmExternRef], []);
let kSig_v_a = makeSig([kWasmAnyFunc], []);
let kSig_v_rr = makeSig([kWasmExternRef, kWasmExternRef], []);
let kSig_v_aa = makeSig([kWasmAnyFunc, kWasmAnyFunc], []);
let kSig_r_v = makeSig([], [kWasmExternRef]);
let kSig_a_v = makeSig([], [kWasmAnyFunc]);
let kSig_a_i = makeSig([kWasmI32], [kWasmAnyFunc]);

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
let kExprCatchAll = 0x19;
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
let kExprLocalGet = 0x20;
let kExprLocalSet = 0x21;
let kExprLocalTee = 0x22;
let kExprGlobalGet = 0x23;
let kExprGlobalSet = 0x24;
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
let kGCPrefix = 0xfb;
let kNumericPrefix = 0xfc;
let kSimdPrefix = 0xfd;
let kAtomicPrefix = 0xfe;

// Use these for multi-byte instructions (opcode > 0x7F needing two LEB bytes):
function GCInstr(opcode) {
  if (opcode <= 0x7F) return [kGCPrefix, opcode];
  return [kGCPrefix, 0x80 | (opcode & 0x7F), opcode >> 7];
}

// GC opcodes
let kExprStructNew = 0x00;
let kExprStructNewDefault = 0x01;
let kExprStructGet = 0x02;
let kExprStructGetS = 0x03;
let kExprStructGetU = 0x04;
let kExprStructSet = 0x05;
let kExprArrayNew = 0x06;
let kExprArrayNewDefault = 0x07;
let kExprArrayNewFixed = 0x08;
let kExprArrayNewData = 0x09;
let kExprArrayNewElem = 0x0a;
let kExprArrayGet = 0x0b;
let kExprArrayGetS = 0x0c;
let kExprArrayGetU = 0x0d;
let kExprArraySet = 0x0e;
let kExprArrayLen = 0x0f;
let kExprArrayFill = 0x10;
let kExprArrayCopy = 0x11;
let kExprArrayInitData = 0x12;
let kExprArrayInitElem = 0x13;
let kExprRefTest = 0x14;
let kExprRefTestNull = 0x15;
let kExprRefCast = 0x16;
let kExprRefCastNull = 0x17;
let kExprBrOnCast = 0x18;
let kExprBrOnCastFail = 0x19;
let kExprExternInternalize = 0x1a;
let kExprExternExternalize = 0x1b;
let kExprI31New = 0x1c;
let kExprI31GetS = 0x1d;
let kExprI31GetU = 0x1e;

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
let kExprS128LoadMem = 0x00;
let kExprS128StoreMem = 0x01;
let kExprI32x4Splat = 0x0c;
let kExprI32x4Eq = 0x2c;
let kExprS1x4AllTrue = 0x75;
let kExprF32x4Min = 0x9e;

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

  emit_heap_type(heap_type) {
    this.emit_bytes(wasmSignedLeb(heap_type, kMaxVarInt32Size));
  }

  emit_type(type) {
    if ((typeof type) == 'number') {
      this.emit_u8(type >= 0 ? type : type & kLeb128Mask);
    } else {
      this.emit_u8(type.opcode);
      if ('depth' in type) this.emit_u8(type.depth);
      this.emit_heap_type(type.heap_type);
    }
  }

  emit_init_expr(expr) {
    this.emit_bytes(expr);
    this.emit_u8(kExprEnd);
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
  constructor(module, type, mutable, init) {
    this.module = module;
    this.type = type;
    this.mutable = mutable;
    this.init = init;
  }

  exportAs(name) {
    this.module.exports.push({name: name, kind: kExternalGlobal,
                              index: this.index});
    return this;
  }
}

function checkExpr(expr) {
  for (let b of expr) {
    if (typeof b !== 'number' || (b & (~0xFF)) !== 0) {
      throw new Error(
          'invalid body (entries must be 8 bit numbers): ' + expr);
    }
  }
}

class WasmTableBuilder {
  constructor(module, type, initial_size, max_size, init_expr) {
    this.module = module;
    this.type = type;
    this.initial_size = initial_size;
    this.has_max = max_size != undefined;
    this.max_size = max_size;
    this.init_expr = init_expr;
    this.has_init = init_expr !== undefined;
  }

  exportAs(name) {
    this.module.exports.push({name: name, kind: kExternalTable,
                              index: this.index});
    return this;
  }
}

function makeField(type, mutability) {
  if ((typeof mutability) != 'boolean') {
    throw new Error('field mutability must be boolean');
  }
  return {type: type, mutability: mutability};
}

class WasmStruct {
  constructor(fields, is_final, supertype_idx) {
    if (!Array.isArray(fields)) {
      throw new Error('struct fields must be an array');
    }
    this.fields = fields;
    this.type_form = kWasmStructTypeForm;
    this.is_final = is_final;
    this.supertype = supertype_idx;
  }
}

class WasmArray {
  constructor(type, mutability, is_final, supertype_idx) {
    this.type = type;
    this.mutability = mutability;
    this.type_form = kWasmArrayTypeForm;
    this.is_final = is_final;
    this.supertype = supertype_idx;
  }
}

class WasmModuleBuilder {
  constructor() {
    this.types = [];
    this.imports = [];
    this.exports = [];
    this.globals = [];
    this.tables = [];
    this.tags = [];
    this.functions = [];
    this.element_segments = [];
    this.data_segments = [];
    this.explicit = [];
    this.rec_groups = [];
    this.num_imported_funcs = 0;
    this.num_imported_globals = 0;
    this.num_imported_tables = 0;
    this.num_imported_tags = 0;
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
    result.emit_string(name);
    return result.trunc_buffer()
  }

  createCustomSection(name, bytes) {
    name = this.stringToBytes(name);
    var section = new Binary();
    section.emit_u8(kUnknownSectionCode);
    section.emit_u32v(name.length + bytes.length);
    section.emit_bytes(name);
    section.emit_bytes(bytes);
    return section.trunc_buffer();
  }

  addCustomSection(name, bytes) {
    this.explicit.push(this.createCustomSection(name, bytes));
  }

  // We use {is_final = true} so that the MVP syntax is generated for
  // signatures.
  addType(type, supertype_idx = kNoSuperType, is_final = true) {
    var pl = type.params.length;   // should have params
    var rl = type.results.length;  // should have results
    var type_copy = {params: type.params, results: type.results,
                     is_final: is_final, supertype: supertype_idx};
    this.types.push(type_copy);
    return this.types.length - 1;
  }

  addStruct(fields, supertype_idx = kNoSuperType, is_final = false) {
    this.types.push(new WasmStruct(fields, is_final, supertype_idx));
    return this.types.length - 1;
  }

  addArray(type, mutability, supertype_idx = kNoSuperType, is_final = false) {
    this.types.push(new WasmArray(type, mutability, is_final, supertype_idx));
    return this.types.length - 1;
  }

  static defaultFor(type) {
    switch (type) {
      case kWasmI32:
        return wasmI32Const(0);
      case kWasmI64:
        return wasmI64Const(0);
      case kWasmF32:
        return wasmF32Const(0.0);
      case kWasmF64:
        return wasmF64Const(0.0);
      case kWasmS128:
        return [kSimdPrefix, kExprS128Const, ...(new Array(16).fill(0))];
      default:
        if ((typeof type) != 'number' && type.opcode != kWasmRefNull) {
          throw new Error("Non-defaultable type");
        }
        let heap_type = (typeof type) == 'number' ? type : type.heap_type;
        return [kExprRefNull, ...wasmSignedLeb(heap_type, kMaxVarInt32Size)];
    }
  }

  addGlobal(type, mutable, init) {
    if (init === undefined) init = WasmModuleBuilder.defaultFor(type);
    checkExpr(init);
    let glob = new WasmGlobalBuilder(this, type, mutable, init);
    glob.index = this.globals.length + this.num_imported_globals;
    this.globals.push(glob);
    return glob;
  }

  addTable(type, initial_size, max_size = undefined, init_expr = undefined) {
    if (type == kWasmI32 || type == kWasmI64 || type == kWasmF32 ||
        type == kWasmF64 || type == kWasmS128 || type == kWasmStmt) {
      throw new Error('Tables must be of a reference type');
    }
    if (init_expr != undefined) checkExpr(init_expr);
    let table = new WasmTableBuilder(
        this, type, initial_size, max_size, init_expr);
    table.index = this.tables.length + this.num_imported_tables;
    this.tables.push(table);
    return table;
  }

  addTag(type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let tag_index = this.tags.length + this.num_imported_tags;
    this.tags.push(type_index);
    return tag_index;
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

  addImportedTag(module, name, type) {
    if (this.tags.length != 0) {
      throw new Error('Imported tags must be declared before local ones');
    }
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let o = {module: module, name: name, kind: kExternalTag, type: type_index};
    this.imports.push(o);
    return this.num_imported_tags++;
  }

  addExport(name, index) {
    this.exports.push({name: name, kind: kExternalFunction, index: index});
    return this;
  }

  addExportOfKind(name, kind, index) {
    this.exports.push({name: name, kind: kind, index: index});
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

  startRecGroup() {
    this.rec_groups.push({start: this.types.length, size: 0});
  }

  endRecGroup() {
    if (this.rec_groups.length == 0) {
      throw new Error("Did not start a recursive group before ending one")
    }
    let last_element = this.rec_groups[this.rec_groups.length - 1]
    if (last_element.size != 0) {
      throw new Error("Did not start a recursive group before ending one")
    }
    last_element.size = this.types.length - last_element.start;
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
      if (debug) print('emitting types @ ' + binary.length);
      binary.emit_section(kTypeSectionCode, section => {
        let length_with_groups = wasm.types.length;
        for (let group of wasm.rec_groups) {
          length_with_groups -= group.size - 1;
        }
        section.emit_u32v(length_with_groups);

        let rec_group_index = 0;

        for (let i = 0; i < wasm.types.length; i++) {
          if (rec_group_index < wasm.rec_groups.length &&
              wasm.rec_groups[rec_group_index].start == i) {
            section.emit_u8(kWasmRecursiveTypeGroupForm);
            section.emit_u32v(wasm.rec_groups[rec_group_index].size);
            rec_group_index++;
          }

          let type = wasm.types[i];
          if (type.supertype != kNoSuperType) {
            section.emit_u8(type.is_final ? kWasmSubtypeFinalForm
                                          : kWasmSubtypeForm);
            section.emit_u8(1);  // supertype count
            section.emit_u32v(type.supertype);
          } else if (!type.is_final) {
            section.emit_u8(kWasmSubtypeForm);
            section.emit_u8(0);  // no supertypes
          }
          if (type instanceof WasmStruct) {
            section.emit_u8(kWasmStructTypeForm);
            section.emit_u32v(type.fields.length);
            for (let field of type.fields) {
              section.emit_type(field.type);
              section.emit_u8(field.mutability ? 1 : 0);
            }
          } else if (type instanceof WasmArray) {
            section.emit_u8(kWasmArrayTypeForm);
            section.emit_type(type.type);
            section.emit_u8(type.mutability ? 1 : 0);
          } else {
            section.emit_u8(kWasmFunctionTypeForm);
            section.emit_u32v(type.params.length);
            for (let param of type.params) {
              section.emit_type(param);
            }
            section.emit_u32v(type.results.length);
            for (let result of type.results) {
              section.emit_type(result);
            }
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
            section.emit_type(imp.type);
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
            section.emit_type(imp.type);
            var has_max = (typeof imp.maximum) != "undefined";
            section.emit_u8(has_max ? 1 : 0); // flags
            section.emit_u32v(imp.initial); // initial
            if (has_max) section.emit_u32v(imp.maximum); // maximum
          } else if (imp.kind == kExternalTag) {
            section.emit_u32v(kTagAttribute);
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
          section.emit_type(table.type);
          section.emit_u8(table.has_max);
          section.emit_u32v(table.initial_size);
          if (table.has_max) section.emit_u32v(table.max_size);
          if (table.has_init) section.emit_init_expr(table.init_expr);
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
          section.emit_type(global.type);
          section.emit_u8(global.mutable);
          section.emit_init_expr(global.init);
        }
      });
    }

    // Add tags.
    if (wasm.tags.length > 0) {
      if (debug) print("emitting tags @ " + binary.length);
      binary.emit_section(kTagSectionCode, section => {
        section.emit_u32v(wasm.tags.length);
        for (let type of wasm.tags) {
          section.emit_u32v(kTagAttribute);
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
              section.emit_u8(kExprGlobalGet);
            } else {
              section.emit_u8(kExprI32Const);
            }
            section.emit_u32v(init.base);
            section.emit_u8(kExprEnd);
            if (init.table != 0) {
              section.emit_u8(kExternalFunction);
            }
            section.emit_u32v(init.array.length);
            for (let index of init.array) {
              section.emit_u32v(index);
            }
          } else {
            // Passive segment.
            section.emit_u8(kPassiveWithElements);  // flags
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
              local_decls.push({count: l.anyref_count, type: kWasmExternRef});
            }
            if (l.anyfunc_count > 0) {
              local_decls.push({count: l.anyfunc_count, type: kWasmAnyFunc});
            }
          }

          header.emit_u32v(local_decls.length);
          for (let decl of local_decls) {
            header.emit_u32v(decl.count);
            header.emit_type(decl.type);
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
              section.emit_u8(kExprGlobalGet);
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
globalThis.WasmModuleBuilder = WasmModuleBuilder;

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
globalThis.wasmSignedLeb = wasmSignedLeb;

function wasmI32Const(val) {
  return [kExprI32Const, ...wasmSignedLeb(val, 5)];
}
globalThis.wasmI32Const = wasmI32Const;

function wasmF32Const(f) {
  // Write in little-endian order at offset 0.
  data_view.setFloat32(0, f, true);
  return [
    kExprF32Const, byte_view[0], byte_view[1], byte_view[2], byte_view[3]
  ];
}
globalThis.wasmI32Const = wasmI32Const;

function wasmF64Const(f) {
  // Write in little-endian order at offset 0.
  data_view.setFloat64(0, f, true);
  return [
    kExprF64Const, byte_view[0], byte_view[1], byte_view[2],
    byte_view[3], byte_view[4], byte_view[5], byte_view[6], byte_view[7]
  ];
}
globalThis.wasmF64Const = wasmF64Const;
