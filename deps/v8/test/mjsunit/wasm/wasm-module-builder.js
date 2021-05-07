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
var kSpecMaxPages = 65536;
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
let kExceptionSectionCode = 13;  // Exception section (between Memory & Global)

// Name section types
let kModuleNameCode = 0;
let kFunctionNamesCode = 1;
let kLocalNamesCode = 2;

let kWasmFunctionTypeForm = 0x60;
let kWasmStructTypeForm = 0x5f;
let kWasmArrayTypeForm = 0x5e;

let kLimitsNoMaximum = 0x00;
let kLimitsWithMaximum = 0x01;
let kLimitsSharedNoMaximum = 0x02;
let kLimitsSharedWithMaximum = 0x03;
let kLimitsMemory64NoMaximum = 0x04;
let kLimitsMemory64WithMaximum = 0x05;

// Segment flags
let kActiveNoIndex = 0;
let kPassive = 1;
let kActiveWithIndex = 2;
let kDeclarative = 3;
let kPassiveWithElements = 5;
let kDeclarativeWithElements = 7;

// Function declaration flags
let kDeclFunctionName   = 0x01;
let kDeclFunctionImport = 0x02;
let kDeclFunctionLocals = 0x04;
let kDeclFunctionExport = 0x08;

// Value types and related
let kWasmStmt = 0x40;
let kWasmI32 = 0x7f;
let kWasmI64 = 0x7e;
let kWasmF32 = 0x7d;
let kWasmF64 = 0x7c;
let kWasmS128 = 0x7b;
let kWasmI8 = 0x7a;
let kWasmI16 = 0x79;
let kWasmFuncRef = 0x70;
let kWasmAnyFunc = kWasmFuncRef; // Alias named as in the JS API spec
let kWasmExternRef = 0x6f;
let kWasmAnyRef = 0x6e;
let kWasmEqRef = 0x6d;
let kWasmOptRef = 0x6c;
let kWasmRef = 0x6b;
function wasmOptRefType(index) { return {opcode: kWasmOptRef, index: index}; }
function wasmRefType(index) { return {opcode: kWasmRef, index: index}; }
let kWasmI31Ref = 0x6a;
let kWasmRtt = 0x69;
function wasmRtt(index, depth) {
  return {opcode: kWasmRtt, index: index, depth: depth};
}

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
let kSig_s_i = makeSig([kWasmI32], [kWasmS128]);
let kSig_i_s = makeSig([kWasmS128], [kWasmI32]);

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
const kWasmOpcodes = {
  'Unreachable': 0x00,
  'Nop': 0x01,
  'Block': 0x02,
  'Loop': 0x03,
  'If': 0x04,
  'Else': 0x05,
  'Try': 0x06,
  'Catch': 0x07,
  'Throw': 0x08,
  'Rethrow': 0x09,
  'CatchAll': 0x19,
  'Unwind': 0x0a,
  'End': 0x0b,
  'Br': 0x0c,
  'BrIf': 0x0d,
  'BrTable': 0x0e,
  'Return': 0x0f,
  'CallFunction': 0x10,
  'CallIndirect': 0x11,
  'ReturnCall': 0x12,
  'ReturnCallIndirect': 0x13,
  'CallRef': 0x14,
  'ReturnCallRef': 0x15,
  'Let': 0x17,
  'Delegate': 0x18,
  'Drop': 0x1a,
  'Select': 0x1b,
  'SelectWithType': 0x1c,
  'LocalGet': 0x20,
  'LocalSet': 0x21,
  'LocalTee': 0x22,
  'GlobalGet': 0x23,
  'GlobalSet': 0x24,
  'TableGet': 0x25,
  'TableSet': 0x26,
  'I32LoadMem': 0x28,
  'I64LoadMem': 0x29,
  'F32LoadMem': 0x2a,
  'F64LoadMem': 0x2b,
  'I32LoadMem8S': 0x2c,
  'I32LoadMem8U': 0x2d,
  'I32LoadMem16S': 0x2e,
  'I32LoadMem16U': 0x2f,
  'I64LoadMem8S': 0x30,
  'I64LoadMem8U': 0x31,
  'I64LoadMem16S': 0x32,
  'I64LoadMem16U': 0x33,
  'I64LoadMem32S': 0x34,
  'I64LoadMem32U': 0x35,
  'I32StoreMem': 0x36,
  'I64StoreMem': 0x37,
  'F32StoreMem': 0x38,
  'F64StoreMem': 0x39,
  'I32StoreMem8': 0x3a,
  'I32StoreMem16': 0x3b,
  'I64StoreMem8': 0x3c,
  'I64StoreMem16': 0x3d,
  'I64StoreMem32': 0x3e,
  'MemorySize': 0x3f,
  'MemoryGrow': 0x40,
  'I32Const': 0x41,
  'I64Const': 0x42,
  'F32Const': 0x43,
  'F64Const': 0x44,
  'I32Eqz': 0x45,
  'I32Eq': 0x46,
  'I32Ne': 0x47,
  'I32LtS': 0x48,
  'I32LtU': 0x49,
  'I32GtS': 0x4a,
  'I32GtU': 0x4b,
  'I32LeS': 0x4c,
  'I32LeU': 0x4d,
  'I32GeS': 0x4e,
  'I32GeU': 0x4f,
  'I64Eqz': 0x50,
  'I64Eq': 0x51,
  'I64Ne': 0x52,
  'I64LtS': 0x53,
  'I64LtU': 0x54,
  'I64GtS': 0x55,
  'I64GtU': 0x56,
  'I64LeS': 0x57,
  'I64LeU': 0x58,
  'I64GeS': 0x59,
  'I64GeU': 0x5a,
  'F32Eq': 0x5b,
  'F32Ne': 0x5c,
  'F32Lt': 0x5d,
  'F32Gt': 0x5e,
  'F32Le': 0x5f,
  'F32Ge': 0x60,
  'F64Eq': 0x61,
  'F64Ne': 0x62,
  'F64Lt': 0x63,
  'F64Gt': 0x64,
  'F64Le': 0x65,
  'F64Ge': 0x66,
  'I32Clz': 0x67,
  'I32Ctz': 0x68,
  'I32Popcnt': 0x69,
  'I32Add': 0x6a,
  'I32Sub': 0x6b,
  'I32Mul': 0x6c,
  'I32DivS': 0x6d,
  'I32DivU': 0x6e,
  'I32RemS': 0x6f,
  'I32RemU': 0x70,
  'I32And': 0x71,
  'I32Ior': 0x72,
  'I32Xor': 0x73,
  'I32Shl': 0x74,
  'I32ShrS': 0x75,
  'I32ShrU': 0x76,
  'I32Rol': 0x77,
  'I32Ror': 0x78,
  'I64Clz': 0x79,
  'I64Ctz': 0x7a,
  'I64Popcnt': 0x7b,
  'I64Add': 0x7c,
  'I64Sub': 0x7d,
  'I64Mul': 0x7e,
  'I64DivS': 0x7f,
  'I64DivU': 0x80,
  'I64RemS': 0x81,
  'I64RemU': 0x82,
  'I64And': 0x83,
  'I64Ior': 0x84,
  'I64Xor': 0x85,
  'I64Shl': 0x86,
  'I64ShrS': 0x87,
  'I64ShrU': 0x88,
  'I64Rol': 0x89,
  'I64Ror': 0x8a,
  'F32Abs': 0x8b,
  'F32Neg': 0x8c,
  'F32Ceil': 0x8d,
  'F32Floor': 0x8e,
  'F32Trunc': 0x8f,
  'F32NearestInt': 0x90,
  'F32Sqrt': 0x91,
  'F32Add': 0x92,
  'F32Sub': 0x93,
  'F32Mul': 0x94,
  'F32Div': 0x95,
  'F32Min': 0x96,
  'F32Max': 0x97,
  'F32CopySign': 0x98,
  'F64Abs': 0x99,
  'F64Neg': 0x9a,
  'F64Ceil': 0x9b,
  'F64Floor': 0x9c,
  'F64Trunc': 0x9d,
  'F64NearestInt': 0x9e,
  'F64Sqrt': 0x9f,
  'F64Add': 0xa0,
  'F64Sub': 0xa1,
  'F64Mul': 0xa2,
  'F64Div': 0xa3,
  'F64Min': 0xa4,
  'F64Max': 0xa5,
  'F64CopySign': 0xa6,
  'I32ConvertI64': 0xa7,
  'I32SConvertF32': 0xa8,
  'I32UConvertF32': 0xa9,
  'I32SConvertF64': 0xaa,
  'I32UConvertF64': 0xab,
  'I64SConvertI32': 0xac,
  'I64UConvertI32': 0xad,
  'I64SConvertF32': 0xae,
  'I64UConvertF32': 0xaf,
  'I64SConvertF64': 0xb0,
  'I64UConvertF64': 0xb1,
  'F32SConvertI32': 0xb2,
  'F32UConvertI32': 0xb3,
  'F32SConvertI64': 0xb4,
  'F32UConvertI64': 0xb5,
  'F32ConvertF64': 0xb6,
  'F64SConvertI32': 0xb7,
  'F64UConvertI32': 0xb8,
  'F64SConvertI64': 0xb9,
  'F64UConvertI64': 0xba,
  'F64ConvertF32': 0xbb,
  'I32ReinterpretF32': 0xbc,
  'I64ReinterpretF64': 0xbd,
  'F32ReinterpretI32': 0xbe,
  'F64ReinterpretI64': 0xbf,
  'I32SExtendI8': 0xc0,
  'I32SExtendI16': 0xc1,
  'I64SExtendI8': 0xc2,
  'I64SExtendI16': 0xc3,
  'I64SExtendI32': 0xc4,
  'RefNull': 0xd0,
  'RefIsNull': 0xd1,
  'RefFunc': 0xd2
};

function defineWasmOpcode(name, value) {
  if (globalThis.kWasmOpcodeNames === undefined) {
    globalThis.kWasmOpcodeNames = {};
  }
  Object.defineProperty(globalThis, name, {value: value});
  if (globalThis.kWasmOpcodeNames[value] !== undefined) {
    throw new Error(`Duplicate wasm opcode: ${value}. Previous name: ${
        globalThis.kWasmOpcodeNames[value]}, new name: ${name}`);
  }
  globalThis.kWasmOpcodeNames[value] = name;
}
for (let name in kWasmOpcodes) {
  defineWasmOpcode(`kExpr${name}`, kWasmOpcodes[name]);
}

// Prefix opcodes
const kPrefixOpcodes = {
  'GC': 0xfb,
  'Numeric': 0xfc,
  'Simd': 0xfd,
  'Atomic': 0xfe
};
for (let prefix in kPrefixOpcodes) {
  defineWasmOpcode(`k${prefix}Prefix`, kPrefixOpcodes[prefix]);
}

// GC opcodes
let kExprStructNewWithRtt = 0x01;
let kExprStructNewDefault = 0x02;
let kExprStructGet = 0x03;
let kExprStructGetS = 0x04;
let kExprStructGetU = 0x05;
let kExprStructSet = 0x06;
let kExprArrayNewWithRtt = 0x11;
let kExprArrayNewDefault = 0x12;
let kExprArrayGet = 0x13;
let kExprArrayGetS = 0x14;
let kExprArrayGetU = 0x15;
let kExprArraySet = 0x16;
let kExprArrayLen = 0x17;
let kExprI31New = 0x20;
let kExprI31GetS = 0x21;
let kExprI31GetU = 0x22;
let kExprRttCanon = 0x30;
let kExprRttSub = 0x31;
let kExprRefTest = 0x40;
let kExprRefCast = 0x41;
let kExprBrOnCast = 0x42;

// Numeric opcodes.
let kExprI32SConvertSatF32 = 0x00;
let kExprI32UConvertSatF32 = 0x01;
let kExprI32SConvertSatF64 = 0x02;
let kExprI32UConvertSatF64 = 0x03;
let kExprI64SConvertSatF32 = 0x04;
let kExprI64UConvertSatF32 = 0x05;
let kExprI64SConvertSatF64 = 0x06;
let kExprI64UConvertSatF64 = 0x07;
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
let kExprS128Load8x8S = 0x01;
let kExprS128Load8x8U = 0x02;
let kExprS128Load16x4S = 0x03;
let kExprS128Load16x4U = 0x04;
let kExprS128Load32x2S = 0x05;
let kExprS128Load32x2U = 0x06;
let kExprS128Load8Splat = 0x07;
let kExprS128Load16Splat = 0x08;
let kExprS128Load32Splat = 0x09;
let kExprS128Load64Splat = 0x0a;
let kExprS128StoreMem = 0x0b;

let kExprS128Const = 0x0c;
let kExprI8x16Shuffle = 0x0d;

let kExprI8x16Swizzle = 0x0e;
let kExprI8x16Splat = 0x0f;
let kExprI16x8Splat = 0x10;
let kExprI32x4Splat = 0x11;
let kExprI64x2Splat = 0x12;
let kExprF32x4Splat = 0x13;
let kExprF64x2Splat = 0x14;
let kExprI8x16ReplaceLane = 0x17;
let kExprI16x8ExtractLaneS = 0x18;
let kExprI16x8ReplaceLane = 0x1a;
let kExprI32x4ExtractLane = 0x1b;
let kExprI32x4ReplaceLane = 0x1c;
let kExprI64x2ReplaceLane = 0x1e;
let kExprF32x4ReplaceLane = 0x20;
let kExprF64x2ExtractLane = 0x21;
let kExprF64x2ReplaceLane = 0x22;
let kExprI8x16Eq = 0x23;
let kExprI8x16Ne = 0x24;
let kExprI8x16LtS = 0x25;
let kExprI8x16LtU = 0x26;
let kExprI8x16GtS = 0x27;
let kExprI8x16GtU = 0x28;
let kExprI8x16LeS = 0x29;
let kExprI8x16LeU = 0x2a;
let kExprI8x16GeS = 0x2b;
let kExprI8x16GeU = 0x2c;
let kExprI16x8Eq = 0x2d;
let kExprI16x8Ne = 0x2e;
let kExprI16x8LtS = 0x2f;
let kExprI16x8LtU = 0x30;
let kExprI16x8GtS = 0x31;
let kExprI16x8GtU = 0x32;
let kExprI16x8LeS = 0x33;
let kExprI16x8LeU = 0x34;
let kExprI16x8GeS = 0x35;
let kExprI16x8GeU = 0x36;
let kExprI32x4Eq = 0x37;
let kExprI32x4Ne = 0x38;
let kExprI32x4LtS = 0x39;
let kExprI32x4LtU = 0x3a;
let kExprI32x4GtS = 0x3b;
let kExprI32x4GtU = 0x3c;
let kExprI32x4LeS = 0x3d;
let kExprI32x4LeU = 0x3e;
let kExprI32x4GeS = 0x3f;
let kExprI32x4GeU = 0x40;
let kExprF32x4Eq = 0x41;
let kExprF32x4Ne = 0x42;
let kExprF32x4Lt = 0x43;
let kExprF32x4Gt = 0x44;
let kExprF32x4Le = 0x45;
let kExprF32x4Ge = 0x46;
let kExprF64x2Eq = 0x47;
let kExprF64x2Ne = 0x48;
let kExprF64x2Lt = 0x49;
let kExprF64x2Gt = 0x4a;
let kExprF64x2Le = 0x4b;
let kExprF64x2Ge = 0x4c;
let kExprS128Not = 0x4d;
let kExprS128And = 0x4e;
let kExprS128AndNot = 0x4f;
let kExprS128Or = 0x50;
let kExprS128Xor = 0x51;
let kExprS128Select = 0x52;
let kExprI8x16Abs = 0x60;
let kExprI8x16Neg = 0x61;
let kExprV128AnyTrue = 0x62;
let kExprV8x16AllTrue = 0x63;
let kExprI8x16SConvertI16x8 = 0x65;
let kExprI8x16UConvertI16x8 = 0x66;
let kExprI8x16Shl = 0x6b;
let kExprI8x16ShrS = 0x6c;
let kExprI8x16ShrU = 0x6d;
let kExprI8x16Add = 0x6e;
let kExprI8x16AddSatS = 0x6f;
let kExprI8x16AddSatU = 0x70;
let kExprI8x16Sub = 0x71;
let kExprI8x16SubSatS = 0x72;
let kExprI8x16SubSatU = 0x73;
let kExprI8x16MinS = 0x76;
let kExprI8x16MinU = 0x77;
let kExprI8x16MaxS = 0x78;
let kExprI8x16MaxU = 0x79;
let kExprI8x16RoundingAverageU = 0x7b;
let kExprI16x8Abs = 0x80;
let kExprI16x8Neg = 0x81;
let kExprV16x8AllTrue = 0x83;
let kExprI16x8SConvertI32x4 = 0x85;
let kExprI16x8UConvertI32x4 = 0x86;
let kExprI16x8SConvertI8x16Low = 0x87;
let kExprI16x8SConvertI8x16High = 0x88;
let kExprI16x8UConvertI8x16Low = 0x89;
let kExprI16x8UConvertI8x16High = 0x8a;
let kExprI16x8Shl = 0x8b;
let kExprI16x8ShrS = 0x8c;
let kExprI16x8ShrU = 0x8d;
let kExprI16x8Add = 0x8e;
let kExprI16x8AddSatS = 0x8f;
let kExprI16x8AddSatU = 0x90;
let kExprI16x8Sub = 0x91;
let kExprI16x8SubSatS = 0x92;
let kExprI16x8SubSatU = 0x93;
let kExprI16x8Mul = 0x95;
let kExprI16x8MinS = 0x96;
let kExprI16x8MinU = 0x97;
let kExprI16x8MaxS = 0x98;
let kExprI16x8MaxU = 0x99;
let kExprI16x8RoundingAverageU = 0x9b;
let kExprI32x4Abs = 0xa0;
let kExprI32x4Neg = 0xa1;
let kExprV32x4AllTrue = 0xa3;
let kExprI32x4SConvertI16x8Low = 0xa7;
let kExprI32x4SConvertI16x8High = 0xa8;
let kExprI32x4UConvertI16x8Low = 0xa9;
let kExprI32x4UConvertI16x8High = 0xaa;
let kExprI32x4Shl = 0xab;
let kExprI32x4ShrS = 0xac;
let kExprI32x4ShrU = 0xad;
let kExprI32x4Add = 0xae;
let kExprI32x4Sub = 0xb1;
let kExprI32x4Mul = 0xb5;
let kExprI32x4MinS = 0xb6;
let kExprI32x4MinU = 0xb7;
let kExprI32x4MaxS = 0xb8;
let kExprI32x4MaxU = 0xb9;
let kExprI64x2Neg = 0xc1;
let kExprI64x2Shl = 0xcb;
let kExprI64x2ShrS = 0xcc;
let kExprI64x2ShrU = 0xcd;
let kExprI64x2Add = 0xce;
let kExprI64x2Sub = 0xd1;
let kExprI64x2Mul = 0xd5;
let kExprI64x2ExtMulHighI32x4U = 0xd7;
let kExprF32x4Abs = 0xe0;
let kExprF32x4Neg = 0xe1;
let kExprF32x4Sqrt = 0xe3;
let kExprF32x4Add = 0xe4;
let kExprF32x4Sub = 0xe5;
let kExprF32x4Mul = 0xe6;
let kExprF32x4Div = 0xe7;
let kExprF32x4Min = 0xe8;
let kExprF32x4Max = 0xe9;
let kExprF64x2Abs = 0xec;
let kExprF64x2Neg = 0xed;
let kExprF64x2Sqrt = 0xef;
let kExprF64x2Add = 0xf0;
let kExprF64x2Sub = 0xf1;
let kExprF64x2Mul = 0xf2;
let kExprF64x2Div = 0xf3;
let kExprF64x2Min = 0xf4;
let kExprF64x2Max = 0xf5;
let kExprI32x4SConvertF32x4 = 0xf8;
let kExprI32x4UConvertF32x4 = 0xf9;
let kExprF32x4SConvertI32x4 = 0xfa;
let kExprF32x4UConvertI32x4 = 0xfb;

// Compilation hint constants.
let kCompilationHintStrategyDefault = 0x00;
let kCompilationHintStrategyLazy = 0x01;
let kCompilationHintStrategyEager = 0x02;
let kCompilationHintStrategyLazyBaselineEagerTopTier = 0x03;
let kCompilationHintTierDefault = 0x00;
let kCompilationHintTierBaseline = 0x01;
let kCompilationHintTierOptimized = 0x02;

let kTrapUnreachable          = 0;
let kTrapMemOutOfBounds       = 1;
let kTrapDivByZero            = 2;
let kTrapDivUnrepresentable   = 3;
let kTrapRemByZero            = 4;
let kTrapFloatUnrepresentable = 5;
let kTrapTableOutOfBounds     = 6;
let kTrapFuncSigMismatch      = 7;
let kTrapUnalignedAccess      = 8;
let kTrapDataSegmentDropped   = 9;
let kTrapElemSegmentDropped   = 10;
let kTrapRethrowNull          = 11;

let kTrapMsgs = [
  "unreachable",
  "memory access out of bounds",
  "divide by zero",
  "divide result unrepresentable",
  "remainder by zero",
  "float unrepresentable in integer range",
  "table index is out of bounds",
  "function signature mismatch",
  "operation does not support unaligned accesses",
  "data segment has been dropped",
  "element segment has been dropped",
  "rethrowing null value"
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

  emit_type(type) {
    if ((typeof type) == "number") this.emit_u8(type);
    else {
      this.emit_u8(type.opcode);
      if ('depth' in type) this.emit_u8(type.depth);
      this.emit_u32v(type.index);
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
  // Encoding of local names: a string corresponds to a local name,
  // a number n corresponds to n undefined names.
  constructor(module, name, type_index, arg_names) {
    this.module = module;
    this.name = name;
    this.type_index = type_index;
    this.body = [];
    this.locals = [];
    this.local_names = arg_names;
    this.body_offset = undefined;  // Not valid until module is serialized.
  }

  numLocalNames() {
    let num_local_names = 0;
    for (let loc_name of this.local_names) {
      if (typeof loc_name == "string") ++num_local_names;
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
      total_locals += l.count
    }
    return total_locals;
  }

  addLocals(type, count, names) {
    this.locals.push({type: type, count: count});
    names = names || [];
    if (names.length > count) throw new Error('too many locals names given');
    this.local_names.push(...names);
    if (count > names.length) this.local_names.push(count - names.length);
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

function makeField(type, mutability) {
  assertEquals("boolean", typeof mutability,
               "field mutability must be boolean");
  return {type: type, mutability: mutability};
}

class WasmStruct {
  constructor(fields) {
    assertTrue(Array.isArray(fields), "struct fields must be an array");
    this.fields = fields;
  }
}

class WasmArray {
  constructor(type) {
    this.type = type;
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

  addMemory(min, max, exported, shared) {
    this.memory = {
      min: min,
      max: max,
      exported: exported,
      shared: shared || false,
      is_memory64: false
    };
    return this;
  }

  addMemory64(min, max, exported) {
    this.memory = {
      min: min,
      max: max,
      exported: exported,
      shared: false,
      is_memory64: true
    };
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

  addStruct(fields) {
    this.types.push(new WasmStruct(fields));
    return this.types.length - 1;
  }

  addArray(type) {
    this.types.push(new WasmArray(type));
    return this.types.length - 1;
  }

  addGlobal(type, mutable) {
    let glob = new WasmGlobalBuilder(this, type, mutable);
    glob.index = this.globals.length + this.num_imported_globals;
    this.globals.push(glob);
    return glob;
  }

  addTable(type, initial_size, max_size = undefined) {
    if (type == kWasmI32 || type == kWasmI64 || type == kWasmF32 ||
        type == kWasmF64 || type == kWasmS128 || type == kWasmStmt) {
      throw new Error('Tables must be of a reference type');
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

  addFunction(name, type, arg_names) {
    arg_names = arg_names || [];
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let num_args = this.types[type_index].params.length;
    if (num_args < arg_names.length) throw new Error("too many arg names provided");
    if (num_args > arg_names.length) arg_names.push(num_args - arg_names.length);
    let func = new WasmFunctionBuilder(this, name, type_index, arg_names);
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
                       type_index: type_index});
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
             maximum: maximum, type: type || kWasmFuncRef};
    this.imports.push(o);
    return this.num_imported_tables++;
  }

  addImportedException(module, name, type) {
    if (this.exceptions.length != 0) {
      throw new Error('Imported exceptions must be declared before local ones');
    }
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let o = {module: module, name: name, kind: kExternalException,
             type_index: type_index};
    this.imports.push(o);
    return this.num_imported_exceptions++;
  }

  addExport(name, index) {
    this.exports.push({name: name, kind: kExternalFunction, index: index});
    return this;
  }

  addExportOfKind(name, kind, index) {
    if (index == undefined && kind != kExternalTable &&
        kind != kExternalMemory) {
      throw new Error(
        'Index for exports other than tables/memories must be provided');
    }
    if (index !== undefined && (typeof index) != 'number') {
      throw new Error('Index for exports must be a number')
    }
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
    this.element_segments.push({
      table: table,
      base: base,
      is_global: is_global,
      array: array,
      is_active: true,
      is_declarative: false
    });
    return this;
  }

  addPassiveElementSegment(array) {
    this.element_segments.push(
        {array: array, is_active: false, is_declarative: false});
    return this;
  }

  addDeclarativeElementSegment(array) {
    this.element_segments.push(
        {array: array, is_active: false, is_declarative: true});
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
            section.emit_u8(1); // Only mutable arrays supported currently.
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
            section.emit_u32v(imp.type_index);
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
          } else if (imp.kind == kExternalException) {
            section.emit_u32v(kExceptionAttribute);
            section.emit_u32v(imp.type_index);
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
        }
      });
    }

    // Add memory section
    if (wasm.memory !== undefined) {
      if (debug) print("emitting memory @ " + binary.length);
      binary.emit_section(kMemorySectionCode, section => {
        section.emit_u8(1);  // one memory entry
        const has_max = wasm.memory.max !== undefined;
        if (wasm.memory.is_memory64) {
          assertFalse(
              wasm.memory.shared, 'sharing memory64 is not supported (yet)');
          section.emit_u8(
              has_max ? kLimitsMemory64WithMaximum : kLimitsMemory64NoMaximum);
          section.emit_u64v(wasm.memory.min);
          if (has_max) section.emit_u64v(wasm.memory.max);
        } else {
          section.emit_u8(
              wasm.memory.shared ?
                  (has_max ? kLimitsSharedWithMaximum :
                             kLimitsSharedNoMaximum) :
                  (has_max ? kLimitsWithMaximum : kLimitsNoMaximum));
          section.emit_u32v(wasm.memory.min);
          if (has_max) section.emit_u32v(wasm.memory.max);
        }
      });
    }

    // Add event section.
    if (wasm.exceptions.length > 0) {
      if (debug) print("emitting events @ " + binary.length);
      binary.emit_section(kExceptionSectionCode, section => {
        section.emit_u32v(wasm.exceptions.length);
        for (let type_index of wasm.exceptions) {
          section.emit_u32v(kExceptionAttribute);
          section.emit_u32v(type_index);
        }
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
              section.emit_bytes(wasmF32Const(global.init));
              break;
            case kWasmF64:
              section.emit_bytes(wasmF64Const(global.init));
              break;
            case kWasmS128:
              section.emit_bytes(wasmS128Const(global.init));
              break;
            case kWasmExternRef:
              section.emit_u8(kExprRefNull);
              section.emit_u8(kWasmExternRef);
              assertEquals(global.function_index, undefined);
              break;
            case kWasmAnyFunc:
              if (global.function_index !== undefined) {
                section.emit_u8(kExprRefFunc);
                section.emit_u32v(global.function_index);
              } else {
                section.emit_u8(kExprRefNull);
                section.emit_u8(kWasmAnyFunc);
              }
              break;
            default:
              if (global.function_index !== undefined) {
                section.emit_u8(kExprRefFunc);
                section.emit_u32v(global.function_index);
              } else {
                section.emit_u8(kExprRefNull);
                section.emit_u32v(global.type.index);
              }
              break;
            }
          } else {
            // Emit a global-index initializer.
            section.emit_u8(kExprGlobalGet);
            section.emit_u32v(global.init_index);
          }
          section.emit_u8(kExprEnd);  // end of init expression
        }
      });
    }

    // Add export table.
    var mem_export = (wasm.memory !== undefined && wasm.memory.exported);
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
          } else if (
              init.is_declarative &&
              init.array.every(index => index !== null)) {
            section.emit_u8(kDeclarative);
            section.emit_u8(kExternalFunction);
            section.emit_u32v(init.array.length);
            for (let index of init.array) {
              section.emit_u32v(index);
            }
          } else {
            // Passive or declarative segment with elements.
            section.emit_u8(
                init.is_declarative ? kDeclarativeWithElements :
                                      kPassiveWithElements);  // flags
            section.emit_u8(kWasmAnyFunc);
            section.emit_u32v(init.array.length);
            for (let index of init.array) {
              if (index === null) {
                section.emit_u8(kExprRefNull);
                section.emit_u8(kWasmAnyFunc);
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
      let section_length = 0;
      binary.emit_section(kCodeSectionCode, section => {
        section.emit_u32v(wasm.functions.length);
        let header = new Binary;
        for (let func of wasm.functions) {
          header.reset();
          // Function body length will be patched later.
          let local_decls = func.locals || [];
          header.emit_u32v(local_decls.length);
          for (let decl of local_decls) {
            header.emit_u32v(decl.count);
            header.emit_type(decl.type);
          }
          section.emit_u32v(header.length + func.body.length);
          section.emit_bytes(header.trunc_buffer());
          // Set to section offset for now, will update.
          func.body_offset = section.length;
          section.emit_bytes(func.body);
        }
        section_length = section.length;
      });
      for (let func of wasm.functions) {
        func.body_offset += binary.length - section_length;
      }
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
              let name_index = 0;
              for (let i = 0; i < func.local_names.length; ++i) {
                if (typeof func.local_names[i] == "string") {
                  name_section.emit_u32v(name_index);
                  name_section.emit_string(func.local_names[i]);
                  name_index++;
                } else {
                  name_index += func.local_names[i];
                }
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
  // Write in little-endian order at offset 0.
  data_view.setFloat32(0, f, true);
  return [
    kExprF32Const, byte_view[0], byte_view[1], byte_view[2], byte_view[3]
  ];
}

function wasmF64Const(f) {
  // Write in little-endian order at offset 0.
  data_view.setFloat64(0, f, true);
  return [
    kExprF64Const, byte_view[0], byte_view[1], byte_view[2],
    byte_view[3], byte_view[4], byte_view[5], byte_view[6], byte_view[7]
  ];
}

function wasmS128Const(f) {
  // Write in little-endian order at offset 0.
  return [kSimdPrefix, kExprS128Const, ...f];
}

function getOpcodeName(opcode) {
  return globalThis.kWasmOpcodeNames?.[opcode] ?? 'unknown';
}
