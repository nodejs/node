'use strict';
const common = require('../../common');
const assert = require('assert');
const test_number = require(`./build/${common.buildType}/test_number`);


// testing api calls for number
assert.strictEqual(0, test_number.Test(0));
assert.strictEqual(1, test_number.Test(1));
assert.strictEqual(-1, test_number.Test(-1));
assert.strictEqual(100, test_number.Test(100));
assert.strictEqual(2121, test_number.Test(2121));
assert.strictEqual(-1233, test_number.Test(-1233));
assert.strictEqual(986583, test_number.Test(986583));
assert.strictEqual(-976675, test_number.Test(-976675));

const num1 = 98765432213456789876546896323445679887645323232436587988766545658;
assert.strictEqual(num1, test_number.Test(num1));

const num2 = -4350987086545760976737453646576078997096876957864353245245769809;
assert.strictEqual(num2, test_number.Test(num2));

const num3 = Number.MAX_SAFE_INTEGER;
assert.strictEqual(num3, test_number.Test(num3));

const num4 = Number.MAX_SAFE_INTEGER + 10;
assert.strictEqual(num4, test_number.Test(num4));

const num5 = Number.MAX_VALUE;
assert.strictEqual(num5, test_number.Test(num5));

const num6 = Number.MAX_VALUE + 10;
assert.strictEqual(num6, test_number.Test(num6));

const num7 = Number.POSITIVE_INFINITY;
assert.strictEqual(num7, test_number.Test(num7));

const num8 = Number.NEGATIVE_INFINITY;
assert.strictEqual(num8, test_number.Test(num8));


// validate documented behaviour when value is retrieved
// as 32 bit integer with napi_get_value_int32
assert.strictEqual(1, test_number.TestInt32Truncation(4294967297));
assert.strictEqual(0, test_number.TestInt32Truncation(4294967296));
assert.strictEqual(-1, test_number.TestInt32Truncation(4294967295));
assert.strictEqual(3, test_number.TestInt32Truncation(4294967296 * 5 + 3));
