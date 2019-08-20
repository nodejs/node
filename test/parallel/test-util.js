// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
// Flags: --expose-internals
require('../common');
const assert = require('assert');
const util = require('util');
const errors = require('internal/errors');
const context = require('vm').runInNewContext;

// isArray
assert.strictEqual(util.isArray([]), true);
assert.strictEqual(util.isArray(Array()), true);
assert.strictEqual(util.isArray(new Array()), true);
assert.strictEqual(util.isArray(new Array(5)), true);
assert.strictEqual(util.isArray(new Array('with', 'some', 'entries')), true);
assert.strictEqual(util.isArray(context('Array')()), true);
assert.strictEqual(util.isArray({}), false);
assert.strictEqual(util.isArray({ push: function() {} }), false);
assert.strictEqual(util.isArray(/regexp/), false);
assert.strictEqual(util.isArray(new Error()), false);
assert.strictEqual(util.isArray(Object.create(Array.prototype)), false);

// isRegExp
assert.strictEqual(util.isRegExp(/regexp/), true);
assert.strictEqual(util.isRegExp(RegExp(), 'foo'), true);
assert.strictEqual(util.isRegExp(new RegExp()), true);
assert.strictEqual(util.isRegExp(context('RegExp')()), true);
assert.strictEqual(util.isRegExp({}), false);
assert.strictEqual(util.isRegExp([]), false);
assert.strictEqual(util.isRegExp(new Date()), false);
assert.strictEqual(util.isRegExp(Object.create(RegExp.prototype)), false);

// isDate
assert.strictEqual(util.isDate(new Date()), true);
assert.strictEqual(util.isDate(new Date(0), 'foo'), true);
assert.strictEqual(util.isDate(new (context('Date'))()), true);
assert.strictEqual(util.isDate(Date()), false);
assert.strictEqual(util.isDate({}), false);
assert.strictEqual(util.isDate([]), false);
assert.strictEqual(util.isDate(new Error()), false);
assert.strictEqual(util.isDate(Object.create(Date.prototype)), false);

// isError
assert.strictEqual(util.isError(new Error()), true);
assert.strictEqual(util.isError(new TypeError()), true);
assert.strictEqual(util.isError(new SyntaxError()), true);
assert.strictEqual(util.isError(new (context('Error'))()), true);
assert.strictEqual(util.isError(new (context('TypeError'))()), true);
assert.strictEqual(util.isError(new (context('SyntaxError'))()), true);
assert.strictEqual(util.isError({}), false);
assert.strictEqual(util.isError({ name: 'Error', message: '' }), false);
assert.strictEqual(util.isError([]), false);
assert.strictEqual(util.isError(Object.create(Error.prototype)), true);

// isObject
assert.strictEqual(util.isObject({}), true);
assert.strictEqual(util.isObject([]), true);
assert.strictEqual(util.isObject(new Number(3)), true);
assert.strictEqual(util.isObject(Number(4)), false);
assert.strictEqual(util.isObject(1), false);

// isPrimitive
assert.strictEqual(util.isPrimitive({}), false);
assert.strictEqual(util.isPrimitive(new Error()), false);
assert.strictEqual(util.isPrimitive(new Date()), false);
assert.strictEqual(util.isPrimitive([]), false);
assert.strictEqual(util.isPrimitive(/regexp/), false);
assert.strictEqual(util.isPrimitive(function() {}), false);
assert.strictEqual(util.isPrimitive(new Number(1)), false);
assert.strictEqual(util.isPrimitive(new String('bla')), false);
assert.strictEqual(util.isPrimitive(new Boolean(true)), false);
assert.strictEqual(util.isPrimitive(1), true);
assert.strictEqual(util.isPrimitive('bla'), true);
assert.strictEqual(util.isPrimitive(true), true);
assert.strictEqual(util.isPrimitive(undefined), true);
assert.strictEqual(util.isPrimitive(null), true);
assert.strictEqual(util.isPrimitive(Infinity), true);
assert.strictEqual(util.isPrimitive(NaN), true);
assert.strictEqual(util.isPrimitive(Symbol('symbol')), true);

// isBuffer
assert.strictEqual(util.isBuffer('foo'), false);
assert.strictEqual(util.isBuffer(Buffer.from('foo')), true);

// _extend
assert.deepStrictEqual(util._extend({ a: 1 }), { a: 1 });
assert.deepStrictEqual(util._extend({ a: 1 }, []), { a: 1 });
assert.deepStrictEqual(util._extend({ a: 1 }, null), { a: 1 });
assert.deepStrictEqual(util._extend({ a: 1 }, true), { a: 1 });
assert.deepStrictEqual(util._extend({ a: 1 }, false), { a: 1 });
assert.deepStrictEqual(util._extend({ a: 1 }, { b: 2 }), { a: 1, b: 2 });
assert.deepStrictEqual(util._extend({ a: 1, b: 2 }, { b: 3 }), { a: 1, b: 3 });

// deprecated
assert.strictEqual(util.isBoolean(true), true);
assert.strictEqual(util.isBoolean(false), true);
assert.strictEqual(util.isBoolean('string'), false);

assert.strictEqual(util.isNull(null), true);
assert.strictEqual(util.isNull(undefined), false);
assert.strictEqual(util.isNull(), false);
assert.strictEqual(util.isNull('string'), false);

assert.strictEqual(util.isUndefined(undefined), true);
assert.strictEqual(util.isUndefined(), true);
assert.strictEqual(util.isUndefined(null), false);
assert.strictEqual(util.isUndefined('string'), false);

assert.strictEqual(util.isNullOrUndefined(null), true);
assert.strictEqual(util.isNullOrUndefined(undefined), true);
assert.strictEqual(util.isNullOrUndefined(), true);
assert.strictEqual(util.isNullOrUndefined('string'), false);

assert.strictEqual(util.isNumber(42), true);
assert.strictEqual(util.isNumber(), false);
assert.strictEqual(util.isNumber('string'), false);

assert.strictEqual(util.isString('string'), true);
assert.strictEqual(util.isString(), false);
assert.strictEqual(util.isString(42), false);

assert.strictEqual(util.isSymbol(Symbol()), true);
assert.strictEqual(util.isSymbol(), false);
assert.strictEqual(util.isSymbol('string'), false);

assert.strictEqual(util.isFunction(() => {}), true);
assert.strictEqual(util.isFunction(function() {}), true);
assert.strictEqual(util.isFunction(), false);
assert.strictEqual(util.isFunction('string'), false);

{
  assert.strictEqual(util.types.isNativeError(new Error()), true);
  assert.strictEqual(util.types.isNativeError(new TypeError()), true);
  assert.strictEqual(util.types.isNativeError(new SyntaxError()), true);
  assert.strictEqual(util.types.isNativeError(new (context('Error'))()), true);
  assert.strictEqual(
    util.types.isNativeError(new (context('TypeError'))()),
    true
  );
  assert.strictEqual(
    util.types.isNativeError(new (context('SyntaxError'))()),
    true
  );
  assert.strictEqual(util.types.isNativeError({}), false);
  assert.strictEqual(
    util.types.isNativeError({ name: 'Error', message: '' }),
    false
  );
  assert.strictEqual(util.types.isNativeError([]), false);
  assert.strictEqual(
    util.types.isNativeError(Object.create(Error.prototype)),
    false
  );
  assert.strictEqual(
    util.types.isNativeError(new errors.codes.ERR_IPC_CHANNEL_CLOSED()),
    true
  );
}
