'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');
const context = require('vm').runInNewContext;

// isArray
assert.equal(true, util.isArray([]));
assert.equal(true, util.isArray(Array()));
assert.equal(true, util.isArray(new Array()));
assert.equal(true, util.isArray(new Array(5)));
assert.equal(true, util.isArray(new Array('with', 'some', 'entries')));
assert.equal(true, util.isArray(context('Array')()));
assert.equal(false, util.isArray({}));
assert.equal(false, util.isArray({ push: function() {} }));
assert.equal(false, util.isArray(/regexp/));
assert.equal(false, util.isArray(new Error()));
assert.equal(false, util.isArray(Object.create(Array.prototype)));

// isRegExp
assert.equal(true, util.isRegExp(/regexp/));
assert.equal(true, util.isRegExp(RegExp()));
assert.equal(true, util.isRegExp(new RegExp()));
assert.equal(true, util.isRegExp(context('RegExp')()));
assert.equal(false, util.isRegExp({}));
assert.equal(false, util.isRegExp([]));
assert.equal(false, util.isRegExp(new Date()));
assert.equal(false, util.isRegExp(Object.create(RegExp.prototype)));

// isDate
assert.equal(true, util.isDate(new Date()));
assert.equal(true, util.isDate(new Date(0)));
// eslint-disable-next-line new-parens
assert.equal(true, util.isDate(new (context('Date'))));
assert.equal(false, util.isDate(Date()));
assert.equal(false, util.isDate({}));
assert.equal(false, util.isDate([]));
assert.equal(false, util.isDate(new Error()));
assert.equal(false, util.isDate(Object.create(Date.prototype)));

// isError
assert.equal(true, util.isError(new Error()));
assert.equal(true, util.isError(new TypeError()));
assert.equal(true, util.isError(new SyntaxError()));
/* eslint-disable new-parens */
assert.equal(true, util.isError(new (context('Error'))));
assert.equal(true, util.isError(new (context('TypeError'))));
assert.equal(true, util.isError(new (context('SyntaxError'))));
/* eslint-enable */
assert.equal(false, util.isError({}));
assert.equal(false, util.isError({ name: 'Error', message: '' }));
assert.equal(false, util.isError([]));
assert.equal(true, util.isError(Object.create(Error.prototype)));

// isObject
assert.ok(util.isObject({}) === true);

// isPrimitive
assert.equal(false, util.isPrimitive({}));
assert.equal(false, util.isPrimitive(new Error()));
assert.equal(false, util.isPrimitive(new Date()));
assert.equal(false, util.isPrimitive([]));
assert.equal(false, util.isPrimitive(/regexp/));
assert.equal(false, util.isPrimitive(function() {}));
assert.equal(false, util.isPrimitive(new Number(1)));
assert.equal(false, util.isPrimitive(new String('bla')));
assert.equal(false, util.isPrimitive(new Boolean(true)));
assert.equal(true, util.isPrimitive(1));
assert.equal(true, util.isPrimitive('bla'));
assert.equal(true, util.isPrimitive(true));
assert.equal(true, util.isPrimitive(undefined));
assert.equal(true, util.isPrimitive(null));
assert.equal(true, util.isPrimitive(Infinity));
assert.equal(true, util.isPrimitive(NaN));
assert.equal(true, util.isPrimitive(Symbol('symbol')));

// isBuffer
assert.equal(false, util.isBuffer('foo'));
assert.equal(true, util.isBuffer(Buffer.from('foo')));

// _extend
assert.deepStrictEqual(util._extend({a: 1}), {a: 1});
assert.deepStrictEqual(util._extend({a: 1}, []), {a: 1});
assert.deepStrictEqual(util._extend({a: 1}, null), {a: 1});
assert.deepStrictEqual(util._extend({a: 1}, true), {a: 1});
assert.deepStrictEqual(util._extend({a: 1}, false), {a: 1});
assert.deepStrictEqual(util._extend({a: 1}, {b: 2}), {a: 1, b: 2});
assert.deepStrictEqual(util._extend({a: 1, b: 2}, {b: 3}), {a: 1, b: 3});

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
