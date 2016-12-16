'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const symbol = Symbol('foo');

assert.strictEqual(util.format(), '');
assert.strictEqual(util.format(''), '');
assert.strictEqual(util.format([]), '[]');
assert.strictEqual(util.format([0]), '[ 0 ]');
assert.strictEqual(util.format({}), '{}');
assert.strictEqual(util.format({foo: 42}), '{ foo: 42 }');
assert.strictEqual(util.format(null), 'null');
assert.strictEqual(util.format(true), 'true');
assert.strictEqual(util.format(false), 'false');
assert.strictEqual(util.format('test'), 'test');

// CHECKME this is for console.log() compatibility - but is it *right*?
assert.strictEqual(util.format('foo', 'bar', 'baz'), 'foo bar baz');

// ES6 Symbol handling
assert.strictEqual(util.format(symbol), 'Symbol(foo)');
assert.strictEqual(util.format('foo', symbol), 'foo Symbol(foo)');
assert.strictEqual(util.format('%s', symbol), 'Symbol(foo)');
assert.strictEqual(util.format('%j', symbol), 'undefined');
assert.throws(function() {
  util.format('%d', symbol);
}, /^TypeError: Cannot convert a Symbol value to a number$/);

// Number format specifier
assert.strictEqual(util.format('%d'), '%d');
assert.strictEqual(util.format('%d', 42.0), '42');
assert.strictEqual(util.format('%d', 42), '42');
assert.strictEqual(util.format('%d', '42'), '42');
assert.strictEqual(util.format('%d', '42.0'), '42');
assert.strictEqual(util.format('%d', 1.5), '1.5');
assert.strictEqual(util.format('%d', -0.5), '-0.5');
assert.strictEqual(util.format('%d', ''), '0');

// Integer format specifier
assert.strictEqual(util.format('%i'), '%i');
assert.strictEqual(util.format('%i', 42.0), '42');
assert.strictEqual(util.format('%i', 42), '42');
assert.strictEqual(util.format('%i', '42'), '42');
assert.strictEqual(util.format('%i', '42.0'), '42');
assert.strictEqual(util.format('%i', 1.5), '1');
assert.strictEqual(util.format('%i', -0.5), '0');
assert.strictEqual(util.format('%i', ''), 'NaN');

// Float format specifier
assert.strictEqual(util.format('%f'), '%f');
assert.strictEqual(util.format('%f', 42.0), '42');
assert.strictEqual(util.format('%f', 42), '42');
assert.strictEqual(util.format('%f', '42'), '42');
assert.strictEqual(util.format('%f', '42.0'), '42');
assert.strictEqual(util.format('%f', 1.5), '1.5');
assert.strictEqual(util.format('%f', -0.5), '-0.5');
assert.strictEqual(util.format('%f', Math.PI), '3.141592653589793');
assert.strictEqual(util.format('%f', ''), 'NaN');

// String format specifier
assert.strictEqual(util.format('%s'), '%s');
assert.strictEqual(util.format('%s', undefined), 'undefined');
assert.strictEqual(util.format('%s', 'foo'), 'foo');
assert.strictEqual(util.format('%s', 42), '42');
assert.strictEqual(util.format('%s', '42'), '42');

// JSON format specifier
assert.strictEqual(util.format('%j'), '%j');
assert.strictEqual(util.format('%j', 42), '42');
assert.strictEqual(util.format('%j', '42'), '"42"');

// Various format specifiers
assert.strictEqual(util.format('%%s%s', 'foo'), '%sfoo');
assert.strictEqual(util.format('%s:%s'), '%s:%s');
assert.strictEqual(util.format('%s:%s', undefined), 'undefined:%s');
assert.strictEqual(util.format('%s:%s', 'foo'), 'foo:%s');
assert.strictEqual(util.format('%s:%s', 'foo', 'bar'), 'foo:bar');
assert.strictEqual(util.format('%s:%s', 'foo', 'bar', 'baz'), 'foo:bar baz');
assert.strictEqual(util.format('%%%s%%', 'hi'), '%hi%');
assert.strictEqual(util.format('%%%s%%%%', 'hi'), '%hi%%');
assert.strictEqual(util.format('%sbc%%def', 'a'), 'abc%def');
assert.strictEqual(util.format('%d:%d', 12, 30), '12:30');
assert.strictEqual(util.format('%d:%d', 12), '12:%d');
assert.strictEqual(util.format('%d:%d'), '%d:%d');
assert.strictEqual(util.format('o: %j, a: %j', {}, []), 'o: {}, a: []');
assert.strictEqual(util.format('o: %j, a: %j', {}), 'o: {}, a: %j');
assert.strictEqual(util.format('o: %j, a: %j'), 'o: %j, a: %j');

{
  const o = {};
  o.o = o;
  assert.strictEqual(util.format('%j', o), '[Circular]');
}

// Errors
const err = new Error('foo');
assert.strictEqual(util.format(err), err.stack);
class CustomError extends Error {
  constructor(msg) {
    super();
    Object.defineProperty(this, 'message',
                          { value: msg, enumerable: false });
    Object.defineProperty(this, 'name',
                          { value: 'CustomError', enumerable: false });
    Error.captureStackTrace(this, CustomError);
  }
}
const customError = new CustomError('bar');
assert.strictEqual(util.format(customError), customError.stack);
// Doesn't capture stack trace
function BadCustomError(msg) {
  Error.call(this);
  Object.defineProperty(this, 'message',
                        { value: msg, enumerable: false });
  Object.defineProperty(this, 'name',
                        { value: 'BadCustomError', enumerable: false });
}
util.inherits(BadCustomError, Error);
assert.strictEqual(util.format(new BadCustomError('foo')),
                   '[BadCustomError: foo]');
