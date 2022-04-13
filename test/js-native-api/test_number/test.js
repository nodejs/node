'use strict';
const common = require('../../common');
const assert = require('assert');
const test_number = require(`./build/${common.buildType}/test_number`);


// Testing api calls for number
function testNumber(num) {
  assert.strictEqual(num, test_number.Test(num));
}

testNumber(0);
testNumber(-0);
testNumber(1);
testNumber(-1);
testNumber(100);
testNumber(2121);
testNumber(-1233);
testNumber(986583);
testNumber(-976675);

/* eslint-disable no-loss-of-precision */
testNumber(
  98765432213456789876546896323445679887645323232436587988766545658);
testNumber(
  -4350987086545760976737453646576078997096876957864353245245769809);
/* eslint-enable no-loss-of-precision */
testNumber(Number.MIN_SAFE_INTEGER);
testNumber(Number.MAX_SAFE_INTEGER);
testNumber(Number.MAX_SAFE_INTEGER + 10);

testNumber(Number.MIN_VALUE);
testNumber(Number.MAX_VALUE);
testNumber(Number.MAX_VALUE + 10);

testNumber(Number.POSITIVE_INFINITY);
testNumber(Number.NEGATIVE_INFINITY);
testNumber(Number.NaN);

function testUint32(input, expected = input) {
  assert.strictEqual(expected, test_number.TestUint32Truncation(input));
}

// Test zero
testUint32(0.0, 0);
testUint32(-0.0, 0);

// Test overflow scenarios
testUint32(4294967295);
testUint32(4294967296, 0);
testUint32(4294967297, 1);
testUint32(17 * 4294967296 + 1, 1);
testUint32(-1, 0xffffffff);

// Validate documented behavior when value is retrieved as 32-bit integer with
// `napi_get_value_int32`
function testInt32(input, expected = input) {
  assert.strictEqual(expected, test_number.TestInt32Truncation(input));
}

// Test zero
testInt32(0.0, 0);
testInt32(-0.0, 0);

// Test min/max int32 range
testInt32(-Math.pow(2, 31));
testInt32(Math.pow(2, 31) - 1);

// Test overflow scenarios
testInt32(4294967297, 1);
testInt32(4294967296, 0);
testInt32(4294967295, -1);
testInt32(4294967296 * 5 + 3, 3);

// Test min/max safe integer range
testInt32(Number.MIN_SAFE_INTEGER, 1);
testInt32(Number.MAX_SAFE_INTEGER, -1);

// Test within int64_t range (with precision loss)
testInt32(-Math.pow(2, 63) + (Math.pow(2, 9) + 1), 1024);
testInt32(Math.pow(2, 63) - (Math.pow(2, 9) + 1), -1024);

// Test min/max double value
testInt32(-Number.MIN_VALUE, 0);
testInt32(Number.MIN_VALUE, 0);
testInt32(-Number.MAX_VALUE, 0);
testInt32(Number.MAX_VALUE, 0);

// Test outside int64_t range
testInt32(-Math.pow(2, 63) + (Math.pow(2, 9)), 0);
testInt32(Math.pow(2, 63) - (Math.pow(2, 9)), 0);

// Test non-finite numbers
testInt32(Number.POSITIVE_INFINITY, 0);
testInt32(Number.NEGATIVE_INFINITY, 0);
testInt32(Number.NaN, 0);

// Validate documented behavior when value is retrieved as 64-bit integer with
// `napi_get_value_int64`
function testInt64(input, expected = input) {
  assert.strictEqual(expected, test_number.TestInt64Truncation(input));
}

// Both V8 and ChakraCore return a sentinel value of `0x8000000000000000` when
// the conversion goes out of range, but V8 treats it as unsigned in some cases.
const RANGEERROR_POSITIVE = Math.pow(2, 63);
const RANGEERROR_NEGATIVE = -Math.pow(2, 63);

// Test zero
testInt64(0.0, 0);
testInt64(-0.0, 0);

// Test min/max safe integer range
testInt64(Number.MIN_SAFE_INTEGER);
testInt64(Number.MAX_SAFE_INTEGER);

// Test within int64_t range (with precision loss)
testInt64(-Math.pow(2, 63) + (Math.pow(2, 9) + 1));
testInt64(Math.pow(2, 63) - (Math.pow(2, 9) + 1));

// Test min/max double value
testInt64(-Number.MIN_VALUE, 0);
testInt64(Number.MIN_VALUE, 0);
testInt64(-Number.MAX_VALUE, RANGEERROR_NEGATIVE);
testInt64(Number.MAX_VALUE, RANGEERROR_POSITIVE);

// Test outside int64_t range
testInt64(-Math.pow(2, 63) + (Math.pow(2, 9)), RANGEERROR_NEGATIVE);
testInt64(Math.pow(2, 63) - (Math.pow(2, 9)), RANGEERROR_POSITIVE);

// Test non-finite numbers
testInt64(Number.POSITIVE_INFINITY, 0);
testInt64(Number.NEGATIVE_INFINITY, 0);
testInt64(Number.NaN, 0);
