'use strict';
const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_conversions`);

const boolExpected = /boolean was expected/;
const numberExpected = /number was expected/;
const stringExpected = /string was expected/;

const testSym = Symbol('test');

assert.strictEqual(test.asBool(false), false);
assert.strictEqual(test.asBool(true), true);
assert.throws(() => test.asBool(undefined), boolExpected);
assert.throws(() => test.asBool(null), boolExpected);
assert.throws(() => test.asBool(Number.NaN), boolExpected);
assert.throws(() => test.asBool(0), boolExpected);
assert.throws(() => test.asBool(''), boolExpected);
assert.throws(() => test.asBool('0'), boolExpected);
assert.throws(() => test.asBool(1), boolExpected);
assert.throws(() => test.asBool('1'), boolExpected);
assert.throws(() => test.asBool('true'), boolExpected);
assert.throws(() => test.asBool({}), boolExpected);
assert.throws(() => test.asBool([]), boolExpected);
assert.throws(() => test.asBool(testSym), boolExpected);

[test.asInt32, test.asUInt32, test.asInt64].forEach((asInt) => {
  assert.strictEqual(asInt(0), 0);
  assert.strictEqual(asInt(1), 1);
  assert.strictEqual(asInt(1.0), 1);
  assert.strictEqual(asInt(1.1), 1);
  assert.strictEqual(asInt(1.9), 1);
  assert.strictEqual(asInt(0.9), 0);
  assert.strictEqual(asInt(999.9), 999);
  assert.strictEqual(asInt(Number.NaN), 0);
  assert.throws(() => asInt(undefined), numberExpected);
  assert.throws(() => asInt(null), numberExpected);
  assert.throws(() => asInt(false), numberExpected);
  assert.throws(() => asInt(''), numberExpected);
  assert.throws(() => asInt('1'), numberExpected);
  assert.throws(() => asInt({}), numberExpected);
  assert.throws(() => asInt([]), numberExpected);
  assert.throws(() => asInt(testSym), numberExpected);
});

assert.strictEqual(test.asInt32(-1), -1);
assert.strictEqual(test.asInt64(-1), -1);
assert.strictEqual(test.asUInt32(-1), Math.pow(2, 32) - 1);

assert.strictEqual(test.asDouble(0), 0);
assert.strictEqual(test.asDouble(1), 1);
assert.strictEqual(test.asDouble(1.0), 1.0);
assert.strictEqual(test.asDouble(1.1), 1.1);
assert.strictEqual(test.asDouble(1.9), 1.9);
assert.strictEqual(test.asDouble(0.9), 0.9);
assert.strictEqual(test.asDouble(999.9), 999.9);
assert.strictEqual(test.asDouble(-1), -1);
assert.ok(Number.isNaN(test.asDouble(Number.NaN)));
assert.throws(() => test.asDouble(undefined), numberExpected);
assert.throws(() => test.asDouble(null), numberExpected);
assert.throws(() => test.asDouble(false), numberExpected);
assert.throws(() => test.asDouble(''), numberExpected);
assert.throws(() => test.asDouble('1'), numberExpected);
assert.throws(() => test.asDouble({}), numberExpected);
assert.throws(() => test.asDouble([]), numberExpected);
assert.throws(() => test.asDouble(testSym), numberExpected);

assert.strictEqual(test.asString(''), '');
assert.strictEqual(test.asString('test'), 'test');
assert.throws(() => test.asString(undefined), stringExpected);
assert.throws(() => test.asString(null), stringExpected);
assert.throws(() => test.asString(false), stringExpected);
assert.throws(() => test.asString(1), stringExpected);
assert.throws(() => test.asString(1.1), stringExpected);
assert.throws(() => test.asString(Number.NaN), stringExpected);
assert.throws(() => test.asString({}), stringExpected);
assert.throws(() => test.asString([]), stringExpected);
assert.throws(() => test.asString(testSym), stringExpected);

assert.strictEqual(test.toBool(true), true);
assert.strictEqual(test.toBool(1), true);
assert.strictEqual(test.toBool(-1), true);
assert.strictEqual(test.toBool('true'), true);
assert.strictEqual(test.toBool('false'), true);
assert.strictEqual(test.toBool({}), true);
assert.strictEqual(test.toBool([]), true);
assert.strictEqual(test.toBool(testSym), true);
assert.strictEqual(test.toBool(false), false);
assert.strictEqual(test.toBool(undefined), false);
assert.strictEqual(test.toBool(null), false);
assert.strictEqual(test.toBool(0), false);
assert.strictEqual(test.toBool(Number.NaN), false);
assert.strictEqual(test.toBool(''), false);

assert.strictEqual(test.toNumber(0), 0);
assert.strictEqual(test.toNumber(1), 1);
assert.strictEqual(test.toNumber(1.1), 1.1);
assert.strictEqual(test.toNumber(-1), -1);
assert.strictEqual(test.toNumber('0'), 0);
assert.strictEqual(test.toNumber('1'), 1);
assert.strictEqual(test.toNumber('1.1'), 1.1);
assert.strictEqual(test.toNumber([]), 0);
assert.strictEqual(test.toNumber(false), 0);
assert.strictEqual(test.toNumber(null), 0);
assert.strictEqual(test.toNumber(''), 0);
assert.ok(Number.isNaN(test.toNumber(Number.NaN)));
assert.ok(Number.isNaN(test.toNumber({})));
assert.ok(Number.isNaN(test.toNumber(undefined)));
assert.throws(() => test.toNumber(testSym), TypeError);

assert.deepStrictEqual({}, test.toObject({}));
assert.deepStrictEqual({ 'test': 1 }, test.toObject({ 'test': 1 }));
assert.deepStrictEqual([], test.toObject([]));
assert.deepStrictEqual([ 1, 2, 3 ], test.toObject([ 1, 2, 3 ]));
assert.deepStrictEqual(new Boolean(false), test.toObject(false));
assert.deepStrictEqual(new Boolean(true), test.toObject(true));
assert.deepStrictEqual(new String(''), test.toObject(''));
assert.deepStrictEqual(new Number(0), test.toObject(0));
assert.deepStrictEqual(new Number(Number.NaN), test.toObject(Number.NaN));
assert.deepStrictEqual(new Object(testSym), test.toObject(testSym));
assert.notStrictEqual(test.toObject(false), false);
assert.notStrictEqual(test.toObject(true), true);
assert.notStrictEqual(test.toObject(''), '');
assert.notStrictEqual(test.toObject(0), 0);
assert.ok(!Number.isNaN(test.toObject(Number.NaN)));

assert.strictEqual(test.toString(''), '');
assert.strictEqual(test.toString('test'), 'test');
assert.strictEqual(test.toString(undefined), 'undefined');
assert.strictEqual(test.toString(null), 'null');
assert.strictEqual(test.toString(false), 'false');
assert.strictEqual(test.toString(true), 'true');
assert.strictEqual(test.toString(0), '0');
assert.strictEqual(test.toString(1.1), '1.1');
assert.strictEqual(test.toString(Number.NaN), 'NaN');
assert.strictEqual(test.toString({}), '[object Object]');
assert.strictEqual(test.toString({ toString: () => 'test' }), 'test');
assert.strictEqual(test.toString([]), '');
assert.strictEqual(test.toString([ 1, 2, 3 ]), '1,2,3');
assert.throws(() => test.toString(testSym), TypeError);

assert.deepStrictEqual(test.testNull.getValueBool(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A boolean was expected',
});

assert.deepStrictEqual(test.testNull.getValueInt32(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
});

assert.deepStrictEqual(test.testNull.getValueUint32(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
});

assert.deepStrictEqual(test.testNull.getValueInt64(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
});


assert.deepStrictEqual(test.testNull.getValueDouble(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'A number was expected',
});

assert.deepStrictEqual(test.testNull.coerceToBool(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
});

assert.deepStrictEqual(test.testNull.coerceToObject(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
});

assert.deepStrictEqual(test.testNull.coerceToString(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
  inputTypeCheck: 'napi_ok',
});

assert.deepStrictEqual(test.testNull.getValueStringUtf8(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
});

assert.deepStrictEqual(test.testNull.getValueStringLatin1(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
});

assert.deepStrictEqual(test.testNull.getValueStringUtf16(), {
  envIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument',
  wrongTypeIn: 'A string was expected',
  bufAndOutLengthIsNull: 'Invalid argument',
});
