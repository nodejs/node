'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const symbol = Symbol('foo');

assert.equal(util.format(), '');
assert.equal(util.format(''), '');
assert.equal(util.format([]), '[]');
assert.equal(util.format([0]), '[ 0 ]');
assert.equal(util.format({}), '{}');
assert.equal(util.format({foo: 42}), '{ foo: 42 }');
assert.equal(util.format(null), 'null');
assert.equal(util.format(true), 'true');
assert.equal(util.format(false), 'false');
assert.equal(util.format('test'), 'test');

// CHECKME this is for console.log() compatibility - but is it *right*?
assert.equal(util.format('foo', 'bar', 'baz'), 'foo bar baz');

// ES6 Symbol handling
assert.equal(util.format(symbol), 'Symbol(foo)');
assert.equal(util.format('foo', symbol), 'foo Symbol(foo)');
assert.equal(util.format('%s', symbol), 'Symbol(foo)');
assert.equal(util.format('%j', symbol), 'undefined');
assert.throws(function() {
  util.format('%d', symbol);
}, TypeError);

assert.equal(util.format('%d', 42.0), '42');
assert.equal(util.format('%d', 42), '42');
assert.equal(util.format('%s', 42), '42');
assert.equal(util.format('%j', 42), '42');

assert.equal(util.format('%d', '42.0'), '42');
assert.equal(util.format('%d', '42'), '42');
assert.equal(util.format('%s', '42'), '42');
assert.equal(util.format('%j', '42'), '"42"');

assert.equal(util.format('%%s%s', 'foo'), '%sfoo');

assert.equal(util.format('%s'), '%s');
assert.equal(util.format('%s', undefined), 'undefined');
assert.equal(util.format('%s', 'foo'), 'foo');
assert.equal(util.format('%s:%s'), '%s:%s');
assert.equal(util.format('%s:%s', undefined), 'undefined:%s');
assert.equal(util.format('%s:%s', 'foo'), 'foo:%s');
assert.equal(util.format('%s:%s', 'foo', 'bar'), 'foo:bar');
assert.equal(util.format('%s:%s', 'foo', 'bar', 'baz'), 'foo:bar baz');
assert.equal(util.format('%%%s%%', 'hi'), '%hi%');
assert.equal(util.format('%%%s%%%%', 'hi'), '%hi%%');

{
  const o = {};
  o.o = o;
  assert.equal(util.format('%j', o), '[Circular]');
}

// Errors
const err = new Error('foo');
assert.equal(util.format(err), err.stack);
function CustomError(msg) {
  Error.call(this);
  Object.defineProperty(this, 'message',
                        { value: msg, enumerable: false });
  Object.defineProperty(this, 'name',
                        { value: 'CustomError', enumerable: false });
  Error.captureStackTrace(this, CustomError);
}
util.inherits(CustomError, Error);
const customError = new CustomError('bar');
assert.equal(util.format(customError), customError.stack);
// Doesn't capture stack trace
function BadCustomError(msg) {
  Error.call(this);
  Object.defineProperty(this, 'message',
                        { value: msg, enumerable: false });
  Object.defineProperty(this, 'name',
                        { value: 'BadCustomError', enumerable: false });
}
util.inherits(BadCustomError, Error);
assert.equal(util.format(new BadCustomError('foo')), '[BadCustomError: foo]');
