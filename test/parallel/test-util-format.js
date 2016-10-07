'use strict';
require('../common');
var assert = require('assert');
var util = require('util');
var symbol = Symbol('foo');

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
}, TypeError);

assert.strictEqual(util.format('%d', 42.0), '42');
assert.strictEqual(util.format('%d', 42), '42');
assert.strictEqual(util.format('%s', 42), '42');
assert.strictEqual(util.format('%j', 42), '42');

assert.strictEqual(util.format('%d', '42.0'), '42');
assert.strictEqual(util.format('%d', '42'), '42');
assert.strictEqual(util.format('%s', '42'), '42');
assert.strictEqual(util.format('%j', '42'), '"42"');

assert.strictEqual(util.format('%%s%s', 'foo'), '%sfoo');

assert.strictEqual(util.format('%s'), '%s');
assert.strictEqual(util.format('%s', undefined), 'undefined');
assert.strictEqual(util.format('%s', 'foo'), 'foo');
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

(function() {
  var o = {};
  o.o = o;
  assert.strictEqual(util.format('%j', o), '[Circular]');
})();

// Errors
assert.equal(util.format(new Error('foo')), '[Error: foo]');
function CustomError(msg) {
  Error.call(this);
  Object.defineProperty(this, 'message',
                        { value: msg, enumerable: false });
  Object.defineProperty(this, 'name',
                        { value: 'CustomError', enumerable: false });
}
util.inherits(CustomError, Error);
assert.strictEqual(util.format(new CustomError('bar')), '[CustomError: bar]');
