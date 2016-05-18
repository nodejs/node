// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalSIMD = global.SIMD;
var MakeTypeError;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

macro SIMD_FLOAT_TYPES(FUNCTION)
FUNCTION(Float32x4, float32x4, 4)
endmacro

macro SIMD_INT_TYPES(FUNCTION)
FUNCTION(Int32x4, int32x4, 4)
FUNCTION(Int16x8, int16x8, 8)
FUNCTION(Int8x16, int8x16, 16)
endmacro

macro SIMD_UINT_TYPES(FUNCTION)
FUNCTION(Uint32x4, uint32x4, 4)
FUNCTION(Uint16x8, uint16x8, 8)
FUNCTION(Uint8x16, uint8x16, 16)
endmacro

macro SIMD_BOOL_TYPES(FUNCTION)
FUNCTION(Bool32x4, bool32x4, 4)
FUNCTION(Bool16x8, bool16x8, 8)
FUNCTION(Bool8x16, bool8x16, 16)
endmacro

macro SIMD_ALL_TYPES(FUNCTION)
SIMD_FLOAT_TYPES(FUNCTION)
SIMD_INT_TYPES(FUNCTION)
SIMD_UINT_TYPES(FUNCTION)
SIMD_BOOL_TYPES(FUNCTION)
endmacro

macro DECLARE_GLOBALS(NAME, TYPE, LANES)
var GlobalNAME = GlobalSIMD.NAME;
endmacro

SIMD_ALL_TYPES(DECLARE_GLOBALS)

macro DECLARE_COMMON_FUNCTIONS(NAME, TYPE, LANES)
function NAMECheckJS(a) {
  return %NAMECheck(a);
}

function NAMEToString() {
  var value = %_ValueOf(this);
  if (typeof(value) !== 'TYPE') {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "NAME.prototype.toString", this);
  }
  var str = "SIMD.NAME(";
  str += %NAMEExtractLane(value, 0);
  for (var i = 1; i < LANES; i++) {
    str += ", " + %NAMEExtractLane(value, i);
  }
  return str + ")";
}

function NAMEToLocaleString() {
  var value = %_ValueOf(this);
  if (typeof(value) !== 'TYPE') {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "NAME.prototype.toLocaleString", this);
  }
  var str = "SIMD.NAME(";
  str += %NAMEExtractLane(value, 0).toLocaleString();
  for (var i = 1; i < LANES; i++) {
    str += ", " + %NAMEExtractLane(value, i).toLocaleString();
  }
  return str + ")";
}

function NAMEValueOf() {
  var value = %_ValueOf(this);
  if (typeof(value) !== 'TYPE') {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "NAME.prototype.valueOf", this);
  }
  return value;
}

function NAMEExtractLaneJS(instance, lane) {
  return %NAMEExtractLane(instance, lane);
}
endmacro

SIMD_ALL_TYPES(DECLARE_COMMON_FUNCTIONS)

macro DECLARE_SHIFT_FUNCTIONS(NAME, TYPE, LANES)
function NAMEShiftLeftByScalarJS(instance, shift) {
  return %NAMEShiftLeftByScalar(instance, shift);
}

function NAMEShiftRightByScalarJS(instance, shift) {
  return %NAMEShiftRightByScalar(instance, shift);
}
endmacro

SIMD_INT_TYPES(DECLARE_SHIFT_FUNCTIONS)
SIMD_UINT_TYPES(DECLARE_SHIFT_FUNCTIONS)

macro SIMD_SMALL_INT_TYPES(FUNCTION)
FUNCTION(Int16x8)
FUNCTION(Int8x16)
FUNCTION(Uint8x16)
FUNCTION(Uint16x8)
endmacro

macro DECLARE_SMALL_INT_FUNCTIONS(NAME)
function NAMEAddSaturateJS(a, b) {
  return %NAMEAddSaturate(a, b);
}

function NAMESubSaturateJS(a, b) {
  return %NAMESubSaturate(a, b);
}
endmacro

SIMD_SMALL_INT_TYPES(DECLARE_SMALL_INT_FUNCTIONS)

macro DECLARE_SIGNED_FUNCTIONS(NAME, TYPE, LANES)
function NAMENegJS(a) {
  return %NAMENeg(a);
}
endmacro

SIMD_FLOAT_TYPES(DECLARE_SIGNED_FUNCTIONS)
SIMD_INT_TYPES(DECLARE_SIGNED_FUNCTIONS)

macro DECLARE_BOOL_FUNCTIONS(NAME, TYPE, LANES)
function NAMEReplaceLaneJS(instance, lane, value) {
  return %NAMEReplaceLane(instance, lane, value);
}

function NAMEAnyTrueJS(s) {
  return %NAMEAnyTrue(s);
}

function NAMEAllTrueJS(s) {
  return %NAMEAllTrue(s);
}
endmacro

SIMD_BOOL_TYPES(DECLARE_BOOL_FUNCTIONS)

macro SIMD_NUMERIC_TYPES(FUNCTION)
SIMD_FLOAT_TYPES(FUNCTION)
SIMD_INT_TYPES(FUNCTION)
SIMD_UINT_TYPES(FUNCTION)
endmacro

macro DECLARE_NUMERIC_FUNCTIONS(NAME, TYPE, LANES)
function NAMEReplaceLaneJS(instance, lane, value) {
  return %NAMEReplaceLane(instance, lane, TO_NUMBER(value));
}

function NAMESelectJS(selector, a, b) {
  return %NAMESelect(selector, a, b);
}

function NAMEAddJS(a, b) {
  return %NAMEAdd(a, b);
}

function NAMESubJS(a, b) {
  return %NAMESub(a, b);
}

function NAMEMulJS(a, b) {
  return %NAMEMul(a, b);
}

function NAMEMinJS(a, b) {
  return %NAMEMin(a, b);
}

function NAMEMaxJS(a, b) {
  return %NAMEMax(a, b);
}

function NAMEEqualJS(a, b) {
  return %NAMEEqual(a, b);
}

function NAMENotEqualJS(a, b) {
  return %NAMENotEqual(a, b);
}

function NAMELessThanJS(a, b) {
  return %NAMELessThan(a, b);
}

function NAMELessThanOrEqualJS(a, b) {
  return %NAMELessThanOrEqual(a, b);
}

function NAMEGreaterThanJS(a, b) {
  return %NAMEGreaterThan(a, b);
}

function NAMEGreaterThanOrEqualJS(a, b) {
  return %NAMEGreaterThanOrEqual(a, b);
}

function NAMELoadJS(tarray, index) {
  return %NAMELoad(tarray, index);
}

function NAMEStoreJS(tarray, index, a) {
  return %NAMEStore(tarray, index, a);
}
endmacro

SIMD_NUMERIC_TYPES(DECLARE_NUMERIC_FUNCTIONS)

macro SIMD_LOGICAL_TYPES(FUNCTION)
SIMD_INT_TYPES(FUNCTION)
SIMD_UINT_TYPES(FUNCTION)
SIMD_BOOL_TYPES(FUNCTION)
endmacro

macro DECLARE_LOGICAL_FUNCTIONS(NAME, TYPE, LANES)
function NAMEAndJS(a, b) {
  return %NAMEAnd(a, b);
}

function NAMEOrJS(a, b) {
  return %NAMEOr(a, b);
}

function NAMEXorJS(a, b) {
  return %NAMEXor(a, b);
}

function NAMENotJS(a) {
  return %NAMENot(a);
}
endmacro

SIMD_LOGICAL_TYPES(DECLARE_LOGICAL_FUNCTIONS)

macro SIMD_FROM_TYPES(FUNCTION)
FUNCTION(Float32x4, Int32x4)
FUNCTION(Float32x4, Uint32x4)
FUNCTION(Int32x4, Float32x4)
FUNCTION(Int32x4, Uint32x4)
FUNCTION(Uint32x4, Float32x4)
FUNCTION(Uint32x4, Int32x4)
FUNCTION(Int16x8, Uint16x8)
FUNCTION(Uint16x8, Int16x8)
FUNCTION(Int8x16, Uint8x16)
FUNCTION(Uint8x16, Int8x16)
endmacro

macro DECLARE_FROM_FUNCTIONS(TO, FROM)
function TOFromFROMJS(a) {
  return %TOFromFROM(a);
}
endmacro

SIMD_FROM_TYPES(DECLARE_FROM_FUNCTIONS)

macro SIMD_FROM_BITS_TYPES(FUNCTION)
FUNCTION(Float32x4, Int32x4)
FUNCTION(Float32x4, Uint32x4)
FUNCTION(Float32x4, Int16x8)
FUNCTION(Float32x4, Uint16x8)
FUNCTION(Float32x4, Int8x16)
FUNCTION(Float32x4, Uint8x16)
FUNCTION(Int32x4, Float32x4)
FUNCTION(Int32x4, Uint32x4)
FUNCTION(Int32x4, Int16x8)
FUNCTION(Int32x4, Uint16x8)
FUNCTION(Int32x4, Int8x16)
FUNCTION(Int32x4, Uint8x16)
FUNCTION(Uint32x4, Float32x4)
FUNCTION(Uint32x4, Int32x4)
FUNCTION(Uint32x4, Int16x8)
FUNCTION(Uint32x4, Uint16x8)
FUNCTION(Uint32x4, Int8x16)
FUNCTION(Uint32x4, Uint8x16)
FUNCTION(Int16x8, Float32x4)
FUNCTION(Int16x8, Int32x4)
FUNCTION(Int16x8, Uint32x4)
FUNCTION(Int16x8, Uint16x8)
FUNCTION(Int16x8, Int8x16)
FUNCTION(Int16x8, Uint8x16)
FUNCTION(Uint16x8, Float32x4)
FUNCTION(Uint16x8, Int32x4)
FUNCTION(Uint16x8, Uint32x4)
FUNCTION(Uint16x8, Int16x8)
FUNCTION(Uint16x8, Int8x16)
FUNCTION(Uint16x8, Uint8x16)
FUNCTION(Int8x16, Float32x4)
FUNCTION(Int8x16, Int32x4)
FUNCTION(Int8x16, Uint32x4)
FUNCTION(Int8x16, Int16x8)
FUNCTION(Int8x16, Uint16x8)
FUNCTION(Int8x16, Uint8x16)
FUNCTION(Uint8x16, Float32x4)
FUNCTION(Uint8x16, Int32x4)
FUNCTION(Uint8x16, Uint32x4)
FUNCTION(Uint8x16, Int16x8)
FUNCTION(Uint8x16, Uint16x8)
FUNCTION(Uint8x16, Int8x16)
endmacro

macro DECLARE_FROM_BITS_FUNCTIONS(TO, FROM)
function TOFromFROMBitsJS(a) {
  return %TOFromFROMBits(a);
}
endmacro

SIMD_FROM_BITS_TYPES(DECLARE_FROM_BITS_FUNCTIONS)


macro SIMD_LOADN_STOREN_TYPES(FUNCTION)
FUNCTION(Float32x4, 1)
FUNCTION(Float32x4, 2)
FUNCTION(Float32x4, 3)
FUNCTION(Int32x4, 1)
FUNCTION(Int32x4, 2)
FUNCTION(Int32x4, 3)
FUNCTION(Uint32x4, 1)
FUNCTION(Uint32x4, 2)
FUNCTION(Uint32x4, 3)
endmacro

macro DECLARE_LOADN_STOREN_FUNCTIONS(NAME, COUNT)
function NAMELoadCOUNTJS(tarray, index) {
  return %NAMELoadCOUNT(tarray, index);
}

function NAMEStoreCOUNTJS(tarray, index, a) {
  return %NAMEStoreCOUNT(tarray, index, a);
}
endmacro

SIMD_LOADN_STOREN_TYPES(DECLARE_LOADN_STOREN_FUNCTIONS)

//-------------------------------------------------------------------

macro SIMD_X4_TYPES(FUNCTION)
FUNCTION(Float32x4)
FUNCTION(Int32x4)
FUNCTION(Uint32x4)
FUNCTION(Bool32x4)
endmacro

macro DECLARE_X4_FUNCTIONS(NAME)
function NAMESplat(s) {
  return %CreateNAME(s, s, s, s);
}

function NAMESwizzleJS(a, c0, c1, c2, c3) {
  return %NAMESwizzle(a, c0, c1, c2, c3);
}

function NAMEShuffleJS(a, b, c0, c1, c2, c3) {
  return %NAMEShuffle(a, b, c0, c1, c2, c3);
}
endmacro

SIMD_X4_TYPES(DECLARE_X4_FUNCTIONS)

macro SIMD_X8_TYPES(FUNCTION)
FUNCTION(Int16x8)
FUNCTION(Uint16x8)
FUNCTION(Bool16x8)
endmacro

macro DECLARE_X8_FUNCTIONS(NAME)
function NAMESplat(s) {
  return %CreateNAME(s, s, s, s, s, s, s, s);
}

function NAMESwizzleJS(a, c0, c1, c2, c3, c4, c5, c6, c7) {
  return %NAMESwizzle(a, c0, c1, c2, c3, c4, c5, c6, c7);
}

function NAMEShuffleJS(a, b, c0, c1, c2, c3, c4, c5, c6, c7) {
  return %NAMEShuffle(a, b, c0, c1, c2, c3, c4, c5, c6, c7);
}
endmacro

SIMD_X8_TYPES(DECLARE_X8_FUNCTIONS)

macro SIMD_X16_TYPES(FUNCTION)
FUNCTION(Int8x16)
FUNCTION(Uint8x16)
FUNCTION(Bool8x16)
endmacro

macro DECLARE_X16_FUNCTIONS(NAME)
function NAMESplat(s) {
  return %CreateNAME(s, s, s, s, s, s, s, s, s, s, s, s, s, s, s, s);
}

function NAMESwizzleJS(a, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11,
                          c12, c13, c14, c15) {
  return %NAMESwizzle(a, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11,
                         c12, c13, c14, c15);
}

function NAMEShuffleJS(a, b, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10,
                             c11, c12, c13, c14, c15) {
  return %NAMEShuffle(a, b, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10,
                            c11, c12, c13, c14, c15);
}
endmacro

SIMD_X16_TYPES(DECLARE_X16_FUNCTIONS)

//-------------------------------------------------------------------

function Float32x4Constructor(c0, c1, c2, c3) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Float32x4");
  }
  return %CreateFloat32x4(TO_NUMBER(c0), TO_NUMBER(c1),
                          TO_NUMBER(c2), TO_NUMBER(c3));
}


function Int32x4Constructor(c0, c1, c2, c3) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Int32x4");
  }
  return %CreateInt32x4(TO_NUMBER(c0), TO_NUMBER(c1),
                        TO_NUMBER(c2), TO_NUMBER(c3));
}


function Uint32x4Constructor(c0, c1, c2, c3) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Uint32x4");
  }
  return %CreateUint32x4(TO_NUMBER(c0), TO_NUMBER(c1),
                         TO_NUMBER(c2), TO_NUMBER(c3));
}


function Bool32x4Constructor(c0, c1, c2, c3) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Bool32x4");
  }
  return %CreateBool32x4(c0, c1, c2, c3);
}


function Int16x8Constructor(c0, c1, c2, c3, c4, c5, c6, c7) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Int16x8");
  }
  return %CreateInt16x8(TO_NUMBER(c0), TO_NUMBER(c1),
                        TO_NUMBER(c2), TO_NUMBER(c3),
                        TO_NUMBER(c4), TO_NUMBER(c5),
                        TO_NUMBER(c6), TO_NUMBER(c7));
}


function Uint16x8Constructor(c0, c1, c2, c3, c4, c5, c6, c7) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Uint16x8");
  }
  return %CreateUint16x8(TO_NUMBER(c0), TO_NUMBER(c1),
                         TO_NUMBER(c2), TO_NUMBER(c3),
                         TO_NUMBER(c4), TO_NUMBER(c5),
                         TO_NUMBER(c6), TO_NUMBER(c7));
}


function Bool16x8Constructor(c0, c1, c2, c3, c4, c5, c6, c7) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Bool16x8");
  }
  return %CreateBool16x8(c0, c1, c2, c3, c4, c5, c6, c7);
}


function Int8x16Constructor(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11,
                            c12, c13, c14, c15) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Int8x16");
  }
  return %CreateInt8x16(TO_NUMBER(c0), TO_NUMBER(c1),
                        TO_NUMBER(c2), TO_NUMBER(c3),
                        TO_NUMBER(c4), TO_NUMBER(c5),
                        TO_NUMBER(c6), TO_NUMBER(c7),
                        TO_NUMBER(c8), TO_NUMBER(c9),
                        TO_NUMBER(c10), TO_NUMBER(c11),
                        TO_NUMBER(c12), TO_NUMBER(c13),
                        TO_NUMBER(c14), TO_NUMBER(c15));
}


function Uint8x16Constructor(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11,
                             c12, c13, c14, c15) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Uint8x16");
  }
  return %CreateUint8x16(TO_NUMBER(c0), TO_NUMBER(c1),
                         TO_NUMBER(c2), TO_NUMBER(c3),
                         TO_NUMBER(c4), TO_NUMBER(c5),
                         TO_NUMBER(c6), TO_NUMBER(c7),
                         TO_NUMBER(c8), TO_NUMBER(c9),
                         TO_NUMBER(c10), TO_NUMBER(c11),
                         TO_NUMBER(c12), TO_NUMBER(c13),
                         TO_NUMBER(c14), TO_NUMBER(c15));
}


function Bool8x16Constructor(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11,
                             c12, c13, c14, c15) {
  if (!IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kNotConstructor, "Bool8x16");
  }
  return %CreateBool8x16(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
                         c13, c14, c15);
}


function Float32x4AbsJS(a) {
  return %Float32x4Abs(a);
}


function Float32x4SqrtJS(a) {
  return %Float32x4Sqrt(a);
}


function Float32x4RecipApproxJS(a) {
  return %Float32x4RecipApprox(a);
}


function Float32x4RecipSqrtApproxJS(a) {
  return %Float32x4RecipSqrtApprox(a);
}


function Float32x4DivJS(a, b) {
  return %Float32x4Div(a, b);
}


function Float32x4MinNumJS(a, b) {
  return %Float32x4MinNum(a, b);
}


function Float32x4MaxNumJS(a, b) {
  return %Float32x4MaxNum(a, b);
}


%AddNamedProperty(GlobalSIMD, toStringTagSymbol, 'SIMD', READ_ONLY | DONT_ENUM);

macro SETUP_SIMD_TYPE(NAME, TYPE, LANES)
%SetCode(GlobalNAME, NAMEConstructor);
%FunctionSetPrototype(GlobalNAME, {});
%AddNamedProperty(GlobalNAME.prototype, 'constructor', GlobalNAME,
    DONT_ENUM);
%AddNamedProperty(GlobalNAME.prototype, toStringTagSymbol, 'NAME',
    DONT_ENUM | READ_ONLY);
utils.InstallFunctions(GlobalNAME.prototype, DONT_ENUM, [
  'toLocaleString', NAMEToLocaleString,
  'toString', NAMEToString,
  'valueOf', NAMEValueOf,
]);
endmacro

SIMD_ALL_TYPES(SETUP_SIMD_TYPE)

//-------------------------------------------------------------------

utils.InstallFunctions(GlobalFloat32x4, DONT_ENUM, [
  'splat', Float32x4Splat,
  'check', Float32x4CheckJS,
  'extractLane', Float32x4ExtractLaneJS,
  'replaceLane', Float32x4ReplaceLaneJS,
  'neg', Float32x4NegJS,
  'abs', Float32x4AbsJS,
  'sqrt', Float32x4SqrtJS,
  'reciprocalApproximation', Float32x4RecipApproxJS,
  'reciprocalSqrtApproximation', Float32x4RecipSqrtApproxJS,
  'add', Float32x4AddJS,
  'sub', Float32x4SubJS,
  'mul', Float32x4MulJS,
  'div', Float32x4DivJS,
  'min', Float32x4MinJS,
  'max', Float32x4MaxJS,
  'minNum', Float32x4MinNumJS,
  'maxNum', Float32x4MaxNumJS,
  'lessThan', Float32x4LessThanJS,
  'lessThanOrEqual', Float32x4LessThanOrEqualJS,
  'greaterThan', Float32x4GreaterThanJS,
  'greaterThanOrEqual', Float32x4GreaterThanOrEqualJS,
  'equal', Float32x4EqualJS,
  'notEqual', Float32x4NotEqualJS,
  'select', Float32x4SelectJS,
  'swizzle', Float32x4SwizzleJS,
  'shuffle', Float32x4ShuffleJS,
  'fromInt32x4', Float32x4FromInt32x4JS,
  'fromUint32x4', Float32x4FromUint32x4JS,
  'fromInt32x4Bits', Float32x4FromInt32x4BitsJS,
  'fromUint32x4Bits', Float32x4FromUint32x4BitsJS,
  'fromInt16x8Bits', Float32x4FromInt16x8BitsJS,
  'fromUint16x8Bits', Float32x4FromUint16x8BitsJS,
  'fromInt8x16Bits', Float32x4FromInt8x16BitsJS,
  'fromUint8x16Bits', Float32x4FromUint8x16BitsJS,
  'load', Float32x4LoadJS,
  'load1', Float32x4Load1JS,
  'load2', Float32x4Load2JS,
  'load3', Float32x4Load3JS,
  'store', Float32x4StoreJS,
  'store1', Float32x4Store1JS,
  'store2', Float32x4Store2JS,
  'store3', Float32x4Store3JS,
]);

utils.InstallFunctions(GlobalInt32x4, DONT_ENUM, [
  'splat', Int32x4Splat,
  'check', Int32x4CheckJS,
  'extractLane', Int32x4ExtractLaneJS,
  'replaceLane', Int32x4ReplaceLaneJS,
  'neg', Int32x4NegJS,
  'add', Int32x4AddJS,
  'sub', Int32x4SubJS,
  'mul', Int32x4MulJS,
  'min', Int32x4MinJS,
  'max', Int32x4MaxJS,
  'and', Int32x4AndJS,
  'or', Int32x4OrJS,
  'xor', Int32x4XorJS,
  'not', Int32x4NotJS,
  'shiftLeftByScalar', Int32x4ShiftLeftByScalarJS,
  'shiftRightByScalar', Int32x4ShiftRightByScalarJS,
  'lessThan', Int32x4LessThanJS,
  'lessThanOrEqual', Int32x4LessThanOrEqualJS,
  'greaterThan', Int32x4GreaterThanJS,
  'greaterThanOrEqual', Int32x4GreaterThanOrEqualJS,
  'equal', Int32x4EqualJS,
  'notEqual', Int32x4NotEqualJS,
  'select', Int32x4SelectJS,
  'swizzle', Int32x4SwizzleJS,
  'shuffle', Int32x4ShuffleJS,
  'fromFloat32x4', Int32x4FromFloat32x4JS,
  'fromUint32x4', Int32x4FromUint32x4JS,
  'fromFloat32x4Bits', Int32x4FromFloat32x4BitsJS,
  'fromUint32x4Bits', Int32x4FromUint32x4BitsJS,
  'fromInt16x8Bits', Int32x4FromInt16x8BitsJS,
  'fromUint16x8Bits', Int32x4FromUint16x8BitsJS,
  'fromInt8x16Bits', Int32x4FromInt8x16BitsJS,
  'fromUint8x16Bits', Int32x4FromUint8x16BitsJS,
  'load', Int32x4LoadJS,
  'load1', Int32x4Load1JS,
  'load2', Int32x4Load2JS,
  'load3', Int32x4Load3JS,
  'store', Int32x4StoreJS,
  'store1', Int32x4Store1JS,
  'store2', Int32x4Store2JS,
  'store3', Int32x4Store3JS,
]);

utils.InstallFunctions(GlobalUint32x4, DONT_ENUM, [
  'splat', Uint32x4Splat,
  'check', Uint32x4CheckJS,
  'extractLane', Uint32x4ExtractLaneJS,
  'replaceLane', Uint32x4ReplaceLaneJS,
  'add', Uint32x4AddJS,
  'sub', Uint32x4SubJS,
  'mul', Uint32x4MulJS,
  'min', Uint32x4MinJS,
  'max', Uint32x4MaxJS,
  'and', Uint32x4AndJS,
  'or', Uint32x4OrJS,
  'xor', Uint32x4XorJS,
  'not', Uint32x4NotJS,
  'shiftLeftByScalar', Uint32x4ShiftLeftByScalarJS,
  'shiftRightByScalar', Uint32x4ShiftRightByScalarJS,
  'lessThan', Uint32x4LessThanJS,
  'lessThanOrEqual', Uint32x4LessThanOrEqualJS,
  'greaterThan', Uint32x4GreaterThanJS,
  'greaterThanOrEqual', Uint32x4GreaterThanOrEqualJS,
  'equal', Uint32x4EqualJS,
  'notEqual', Uint32x4NotEqualJS,
  'select', Uint32x4SelectJS,
  'swizzle', Uint32x4SwizzleJS,
  'shuffle', Uint32x4ShuffleJS,
  'fromFloat32x4', Uint32x4FromFloat32x4JS,
  'fromInt32x4', Uint32x4FromInt32x4JS,
  'fromFloat32x4Bits', Uint32x4FromFloat32x4BitsJS,
  'fromInt32x4Bits', Uint32x4FromInt32x4BitsJS,
  'fromInt16x8Bits', Uint32x4FromInt16x8BitsJS,
  'fromUint16x8Bits', Uint32x4FromUint16x8BitsJS,
  'fromInt8x16Bits', Uint32x4FromInt8x16BitsJS,
  'fromUint8x16Bits', Uint32x4FromUint8x16BitsJS,
  'load', Uint32x4LoadJS,
  'load1', Uint32x4Load1JS,
  'load2', Uint32x4Load2JS,
  'load3', Uint32x4Load3JS,
  'store', Uint32x4StoreJS,
  'store1', Uint32x4Store1JS,
  'store2', Uint32x4Store2JS,
  'store3', Uint32x4Store3JS,
]);

utils.InstallFunctions(GlobalBool32x4, DONT_ENUM, [
  'splat', Bool32x4Splat,
  'check', Bool32x4CheckJS,
  'extractLane', Bool32x4ExtractLaneJS,
  'replaceLane', Bool32x4ReplaceLaneJS,
  'and', Bool32x4AndJS,
  'or', Bool32x4OrJS,
  'xor', Bool32x4XorJS,
  'not', Bool32x4NotJS,
  'anyTrue', Bool32x4AnyTrueJS,
  'allTrue', Bool32x4AllTrueJS,
  'swizzle', Bool32x4SwizzleJS,
  'shuffle', Bool32x4ShuffleJS,
]);

utils.InstallFunctions(GlobalInt16x8, DONT_ENUM, [
  'splat', Int16x8Splat,
  'check', Int16x8CheckJS,
  'extractLane', Int16x8ExtractLaneJS,
  'replaceLane', Int16x8ReplaceLaneJS,
  'neg', Int16x8NegJS,
  'add', Int16x8AddJS,
  'sub', Int16x8SubJS,
  'addSaturate', Int16x8AddSaturateJS,
  'subSaturate', Int16x8SubSaturateJS,
  'mul', Int16x8MulJS,
  'min', Int16x8MinJS,
  'max', Int16x8MaxJS,
  'and', Int16x8AndJS,
  'or', Int16x8OrJS,
  'xor', Int16x8XorJS,
  'not', Int16x8NotJS,
  'shiftLeftByScalar', Int16x8ShiftLeftByScalarJS,
  'shiftRightByScalar', Int16x8ShiftRightByScalarJS,
  'lessThan', Int16x8LessThanJS,
  'lessThanOrEqual', Int16x8LessThanOrEqualJS,
  'greaterThan', Int16x8GreaterThanJS,
  'greaterThanOrEqual', Int16x8GreaterThanOrEqualJS,
  'equal', Int16x8EqualJS,
  'notEqual', Int16x8NotEqualJS,
  'select', Int16x8SelectJS,
  'swizzle', Int16x8SwizzleJS,
  'shuffle', Int16x8ShuffleJS,
  'fromUint16x8', Int16x8FromUint16x8JS,
  'fromFloat32x4Bits', Int16x8FromFloat32x4BitsJS,
  'fromInt32x4Bits', Int16x8FromInt32x4BitsJS,
  'fromUint32x4Bits', Int16x8FromUint32x4BitsJS,
  'fromUint16x8Bits', Int16x8FromUint16x8BitsJS,
  'fromInt8x16Bits', Int16x8FromInt8x16BitsJS,
  'fromUint8x16Bits', Int16x8FromUint8x16BitsJS,
  'load', Int16x8LoadJS,
  'store', Int16x8StoreJS,
]);

utils.InstallFunctions(GlobalUint16x8, DONT_ENUM, [
  'splat', Uint16x8Splat,
  'check', Uint16x8CheckJS,
  'extractLane', Uint16x8ExtractLaneJS,
  'replaceLane', Uint16x8ReplaceLaneJS,
  'add', Uint16x8AddJS,
  'sub', Uint16x8SubJS,
  'addSaturate', Uint16x8AddSaturateJS,
  'subSaturate', Uint16x8SubSaturateJS,
  'mul', Uint16x8MulJS,
  'min', Uint16x8MinJS,
  'max', Uint16x8MaxJS,
  'and', Uint16x8AndJS,
  'or', Uint16x8OrJS,
  'xor', Uint16x8XorJS,
  'not', Uint16x8NotJS,
  'shiftLeftByScalar', Uint16x8ShiftLeftByScalarJS,
  'shiftRightByScalar', Uint16x8ShiftRightByScalarJS,
  'lessThan', Uint16x8LessThanJS,
  'lessThanOrEqual', Uint16x8LessThanOrEqualJS,
  'greaterThan', Uint16x8GreaterThanJS,
  'greaterThanOrEqual', Uint16x8GreaterThanOrEqualJS,
  'equal', Uint16x8EqualJS,
  'notEqual', Uint16x8NotEqualJS,
  'select', Uint16x8SelectJS,
  'swizzle', Uint16x8SwizzleJS,
  'shuffle', Uint16x8ShuffleJS,
  'fromInt16x8', Uint16x8FromInt16x8JS,
  'fromFloat32x4Bits', Uint16x8FromFloat32x4BitsJS,
  'fromInt32x4Bits', Uint16x8FromInt32x4BitsJS,
  'fromUint32x4Bits', Uint16x8FromUint32x4BitsJS,
  'fromInt16x8Bits', Uint16x8FromInt16x8BitsJS,
  'fromInt8x16Bits', Uint16x8FromInt8x16BitsJS,
  'fromUint8x16Bits', Uint16x8FromUint8x16BitsJS,
  'load', Uint16x8LoadJS,
  'store', Uint16x8StoreJS,
]);

utils.InstallFunctions(GlobalBool16x8, DONT_ENUM, [
  'splat', Bool16x8Splat,
  'check', Bool16x8CheckJS,
  'extractLane', Bool16x8ExtractLaneJS,
  'replaceLane', Bool16x8ReplaceLaneJS,
  'and', Bool16x8AndJS,
  'or', Bool16x8OrJS,
  'xor', Bool16x8XorJS,
  'not', Bool16x8NotJS,
  'anyTrue', Bool16x8AnyTrueJS,
  'allTrue', Bool16x8AllTrueJS,
  'swizzle', Bool16x8SwizzleJS,
  'shuffle', Bool16x8ShuffleJS,
]);

utils.InstallFunctions(GlobalInt8x16, DONT_ENUM, [
  'splat', Int8x16Splat,
  'check', Int8x16CheckJS,
  'extractLane', Int8x16ExtractLaneJS,
  'replaceLane', Int8x16ReplaceLaneJS,
  'neg', Int8x16NegJS,
  'add', Int8x16AddJS,
  'sub', Int8x16SubJS,
  'addSaturate', Int8x16AddSaturateJS,
  'subSaturate', Int8x16SubSaturateJS,
  'mul', Int8x16MulJS,
  'min', Int8x16MinJS,
  'max', Int8x16MaxJS,
  'and', Int8x16AndJS,
  'or', Int8x16OrJS,
  'xor', Int8x16XorJS,
  'not', Int8x16NotJS,
  'shiftLeftByScalar', Int8x16ShiftLeftByScalarJS,
  'shiftRightByScalar', Int8x16ShiftRightByScalarJS,
  'lessThan', Int8x16LessThanJS,
  'lessThanOrEqual', Int8x16LessThanOrEqualJS,
  'greaterThan', Int8x16GreaterThanJS,
  'greaterThanOrEqual', Int8x16GreaterThanOrEqualJS,
  'equal', Int8x16EqualJS,
  'notEqual', Int8x16NotEqualJS,
  'select', Int8x16SelectJS,
  'swizzle', Int8x16SwizzleJS,
  'shuffle', Int8x16ShuffleJS,
  'fromUint8x16', Int8x16FromUint8x16JS,
  'fromFloat32x4Bits', Int8x16FromFloat32x4BitsJS,
  'fromInt32x4Bits', Int8x16FromInt32x4BitsJS,
  'fromUint32x4Bits', Int8x16FromUint32x4BitsJS,
  'fromInt16x8Bits', Int8x16FromInt16x8BitsJS,
  'fromUint16x8Bits', Int8x16FromUint16x8BitsJS,
  'fromUint8x16Bits', Int8x16FromUint8x16BitsJS,
  'load', Int8x16LoadJS,
  'store', Int8x16StoreJS,
]);

utils.InstallFunctions(GlobalUint8x16, DONT_ENUM, [
  'splat', Uint8x16Splat,
  'check', Uint8x16CheckJS,
  'extractLane', Uint8x16ExtractLaneJS,
  'replaceLane', Uint8x16ReplaceLaneJS,
  'add', Uint8x16AddJS,
  'sub', Uint8x16SubJS,
  'addSaturate', Uint8x16AddSaturateJS,
  'subSaturate', Uint8x16SubSaturateJS,
  'mul', Uint8x16MulJS,
  'min', Uint8x16MinJS,
  'max', Uint8x16MaxJS,
  'and', Uint8x16AndJS,
  'or', Uint8x16OrJS,
  'xor', Uint8x16XorJS,
  'not', Uint8x16NotJS,
  'shiftLeftByScalar', Uint8x16ShiftLeftByScalarJS,
  'shiftRightByScalar', Uint8x16ShiftRightByScalarJS,
  'lessThan', Uint8x16LessThanJS,
  'lessThanOrEqual', Uint8x16LessThanOrEqualJS,
  'greaterThan', Uint8x16GreaterThanJS,
  'greaterThanOrEqual', Uint8x16GreaterThanOrEqualJS,
  'equal', Uint8x16EqualJS,
  'notEqual', Uint8x16NotEqualJS,
  'select', Uint8x16SelectJS,
  'swizzle', Uint8x16SwizzleJS,
  'shuffle', Uint8x16ShuffleJS,
  'fromInt8x16', Uint8x16FromInt8x16JS,
  'fromFloat32x4Bits', Uint8x16FromFloat32x4BitsJS,
  'fromInt32x4Bits', Uint8x16FromInt32x4BitsJS,
  'fromUint32x4Bits', Uint8x16FromUint32x4BitsJS,
  'fromInt16x8Bits', Uint8x16FromInt16x8BitsJS,
  'fromUint16x8Bits', Uint8x16FromUint16x8BitsJS,
  'fromInt8x16Bits', Uint8x16FromInt8x16BitsJS,
  'load', Uint8x16LoadJS,
  'store', Uint8x16StoreJS,
]);

utils.InstallFunctions(GlobalBool8x16, DONT_ENUM, [
  'splat', Bool8x16Splat,
  'check', Bool8x16CheckJS,
  'extractLane', Bool8x16ExtractLaneJS,
  'replaceLane', Bool8x16ReplaceLaneJS,
  'and', Bool8x16AndJS,
  'or', Bool8x16OrJS,
  'xor', Bool8x16XorJS,
  'not', Bool8x16NotJS,
  'anyTrue', Bool8x16AnyTrueJS,
  'allTrue', Bool8x16AllTrueJS,
  'swizzle', Bool8x16SwizzleJS,
  'shuffle', Bool8x16ShuffleJS,
]);

utils.Export(function(to) {
  to.Float32x4ToString = Float32x4ToString;
  to.Int32x4ToString = Int32x4ToString;
  to.Uint32x4ToString = Uint32x4ToString;
  to.Bool32x4ToString = Bool32x4ToString;
  to.Int16x8ToString = Int16x8ToString;
  to.Uint16x8ToString = Uint16x8ToString;
  to.Bool16x8ToString = Bool16x8ToString;
  to.Int8x16ToString = Int8x16ToString;
  to.Uint8x16ToString = Uint8x16ToString;
  to.Bool8x16ToString = Bool8x16ToString;
});

})
