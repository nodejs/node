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
const common = require('../common');
const assert = require('assert');
const util = require('util');
const errors = require('internal/errors');
const binding = process.binding('util');
const context = require('vm').runInNewContext;

// isArray
assert.strictEqual(true, util.isArray([]));
assert.strictEqual(true, util.isArray(Array()));
assert.strictEqual(true, util.isArray(new Array()));
assert.strictEqual(true, util.isArray(new Array(5)));
assert.strictEqual(true, util.isArray(new Array('with', 'some', 'entries')));
assert.strictEqual(true, util.isArray(context('Array')()));
assert.strictEqual(false, util.isArray({}));
assert.strictEqual(false, util.isArray({ push: function() {} }));
assert.strictEqual(false, util.isArray(/regexp/));
assert.strictEqual(false, util.isArray(new Error()));
assert.strictEqual(false, util.isArray(Object.create(Array.prototype)));

// isRegExp
assert.strictEqual(true, util.isRegExp(/regexp/));
assert.strictEqual(true, util.isRegExp(RegExp()));
assert.strictEqual(true, util.isRegExp(new RegExp()));
assert.strictEqual(true, util.isRegExp(context('RegExp')()));
assert.strictEqual(false, util.isRegExp({}));
assert.strictEqual(false, util.isRegExp([]));
assert.strictEqual(false, util.isRegExp(new Date()));
assert.strictEqual(false, util.isRegExp(Object.create(RegExp.prototype)));

// isDate
assert.strictEqual(true, util.isDate(new Date()));
assert.strictEqual(true, util.isDate(new Date(0)));
assert.strictEqual(true, util.isDate(new (context('Date'))()));
assert.strictEqual(false, util.isDate(Date()));
assert.strictEqual(false, util.isDate({}));
assert.strictEqual(false, util.isDate([]));
assert.strictEqual(false, util.isDate(new Error()));
assert.strictEqual(false, util.isDate(Object.create(Date.prototype)));

// isError
assert.strictEqual(true, util.isError(new Error()));
assert.strictEqual(true, util.isError(new TypeError()));
assert.strictEqual(true, util.isError(new SyntaxError()));
assert.strictEqual(true, util.isError(new (context('Error'))()));
assert.strictEqual(true, util.isError(new (context('TypeError'))()));
assert.strictEqual(true, util.isError(new (context('SyntaxError'))()));
assert.strictEqual(false, util.isError({}));
assert.strictEqual(false, util.isError({ name: 'Error', message: '' }));
assert.strictEqual(false, util.isError([]));
assert.strictEqual(true, util.isError(Object.create(Error.prototype)));

// isObject
assert.ok(util.isObject({}) === true);

// isPrimitive
assert.strictEqual(false, util.isPrimitive({}));
assert.strictEqual(false, util.isPrimitive(new Error()));
assert.strictEqual(false, util.isPrimitive(new Date()));
assert.strictEqual(false, util.isPrimitive([]));
assert.strictEqual(false, util.isPrimitive(/regexp/));
assert.strictEqual(false, util.isPrimitive(function() {}));
assert.strictEqual(false, util.isPrimitive(new Number(1)));
assert.strictEqual(false, util.isPrimitive(new String('bla')));
assert.strictEqual(false, util.isPrimitive(new Boolean(true)));
assert.strictEqual(true, util.isPrimitive(1));
assert.strictEqual(true, util.isPrimitive('bla'));
assert.strictEqual(true, util.isPrimitive(true));
assert.strictEqual(true, util.isPrimitive(undefined));
assert.strictEqual(true, util.isPrimitive(null));
assert.strictEqual(true, util.isPrimitive(Infinity));
assert.strictEqual(true, util.isPrimitive(NaN));
assert.strictEqual(true, util.isPrimitive(Symbol('symbol')));

// isBuffer
assert.strictEqual(false, util.isBuffer('foo'));
assert.strictEqual(true, util.isBuffer(Buffer.from('foo')));

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
assert.strictEqual(util.isNull(), false);
assert.strictEqual(util.isNull('string'), false);

assert.strictEqual(util.isUndefined(), true);
assert.strictEqual(util.isUndefined(null), false);
assert.strictEqual(util.isUndefined('string'), false);

assert.strictEqual(util.isNullOrUndefined(null), true);
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

common.expectWarning('DeprecationWarning', [
  'util.print is deprecated. Use console.log instead.',
  'util.puts is deprecated. Use console.log instead.',
  'util.debug is deprecated. Use console.error instead.',
  'util.error is deprecated. Use console.error instead.'
]);

util.print('test');
util.puts('test');
util.debug('test');
util.error('test');

{
  // binding.isNativeError()
  assert.strictEqual(binding.isNativeError(new Error()), true);
  assert.strictEqual(binding.isNativeError(new TypeError()), true);
  assert.strictEqual(binding.isNativeError(new SyntaxError()), true);
  assert.strictEqual(binding.isNativeError(new (context('Error'))()), true);
  assert.strictEqual(binding.isNativeError(new (context('TypeError'))()), true);
  assert.strictEqual(binding.isNativeError(new (context('SyntaxError'))()),
                     true);
  assert.strictEqual(binding.isNativeError({}), false);
  assert.strictEqual(binding.isNativeError({ name: 'Error', message: '' }),
                     false);
  assert.strictEqual(binding.isNativeError([]), false);
  assert.strictEqual(binding.isNativeError(Object.create(Error.prototype)),
                     false);
  assert.strictEqual(
    binding.isNativeError(new errors.Error('ERR_IPC_CHANNEL_CLOSED')),
    true
  );
}
