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
require('../common');
const assert = require('assert');
const util = require('util');
const symbol = Symbol('foo');

assert.strictEqual(util.format(), '');
assert.strictEqual(util.format(''), '');
assert.strictEqual(util.format([]), '[]');
assert.strictEqual(util.format([0]), '[ 0 ]');
assert.strictEqual(util.format({}), '{}');
assert.strictEqual(util.format({ foo: 42 }), '{ foo: 42 }');
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

// Number format specifier
assert.strictEqual(util.format('%d'), '%d');
assert.strictEqual(util.format('%d', 42.0), '42');
assert.strictEqual(util.format('%d', 42), '42');
assert.strictEqual(util.format('%d', '42'), '42');
assert.strictEqual(util.format('%d', '42.0'), '42');
assert.strictEqual(util.format('%d', 1.5), '1.5');
assert.strictEqual(util.format('%d', -0.5), '-0.5');
assert.strictEqual(util.format('%d', ''), '0');
assert.strictEqual(util.format('%d', Symbol()), 'NaN');
assert.strictEqual(util.format('%d %d', 42, 43), '42 43');
assert.strictEqual(util.format('%d %d', 42), '42 %d');
assert.strictEqual(
  util.format('%d', 1180591620717411303424),
  '1.1805916207174113e+21'
);
assert.strictEqual(
  util.format('%d', 1180591620717411303424n),
  '1180591620717411303424n'
);
assert.strictEqual(
  util.format('%d %d', 1180591620717411303424n, 12345678901234567890123n),
  '1180591620717411303424n 12345678901234567890123n'
);

// Integer format specifier
assert.strictEqual(util.format('%i'), '%i');
assert.strictEqual(util.format('%i', 42.0), '42');
assert.strictEqual(util.format('%i', 42), '42');
assert.strictEqual(util.format('%i', '42'), '42');
assert.strictEqual(util.format('%i', '42.0'), '42');
assert.strictEqual(util.format('%i', 1.5), '1');
assert.strictEqual(util.format('%i', -0.5), '0');
assert.strictEqual(util.format('%i', ''), 'NaN');
assert.strictEqual(util.format('%i', Symbol()), 'NaN');
assert.strictEqual(util.format('%i %i', 42, 43), '42 43');
assert.strictEqual(util.format('%i %i', 42), '42 %i');
assert.strictEqual(
  util.format('%i', 1180591620717411303424),
  '1'
);
assert.strictEqual(
  util.format('%i', 1180591620717411303424n),
  '1180591620717411303424n'
);
assert.strictEqual(
  util.format('%i %i', 1180591620717411303424n, 12345678901234567890123n),
  '1180591620717411303424n 12345678901234567890123n'
);

assert.strictEqual(
  util.format('%d %i', 1180591620717411303424n, 12345678901234567890123n),
  '1180591620717411303424n 12345678901234567890123n'
);

assert.strictEqual(
  util.format('%i %d', 1180591620717411303424n, 12345678901234567890123n),
  '1180591620717411303424n 12345678901234567890123n'
);

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
assert.strictEqual(util.format('%f', Symbol('foo')), 'NaN');
assert.strictEqual(util.format('%f', 5n), '5');
assert.strictEqual(util.format('%f %f', 42, 43), '42 43');
assert.strictEqual(util.format('%f %f', 42), '42 %f');

// String format specifier
assert.strictEqual(util.format('%s'), '%s');
assert.strictEqual(util.format('%s', undefined), 'undefined');
assert.strictEqual(util.format('%s', 'foo'), 'foo');
assert.strictEqual(util.format('%s', 42), '42');
assert.strictEqual(util.format('%s', '42'), '42');
assert.strictEqual(util.format('%s %s', 42, 43), '42 43');
assert.strictEqual(util.format('%s %s', 42), '42 %s');

// JSON format specifier
assert.strictEqual(util.format('%j'), '%j');
assert.strictEqual(util.format('%j', 42), '42');
assert.strictEqual(util.format('%j', '42'), '"42"');
assert.strictEqual(util.format('%j %j', 42, 43), '42 43');
assert.strictEqual(util.format('%j %j', 42), '42 %j');

// Object format specifier
const obj = {
  foo: 'bar',
  foobar: 1,
  func: function() {}
};
const nestedObj = {
  foo: 'bar',
  foobar: {
    foo: 'bar',
    func: function() {}
  }
};
const nestedObj2 = {
  foo: 'bar',
  foobar: 1,
  func: [{ a: function() {} }]
};
assert.strictEqual(util.format('%o'), '%o');
assert.strictEqual(util.format('%o', 42), '42');
assert.strictEqual(util.format('%o', 'foo'), '\'foo\'');
assert.strictEqual(
  util.format('%o', obj),
  '{ foo: \'bar\',\n' +
  '  foobar: 1,\n' +
  '  func:\n' +
  '   { [Function: func]\n' +
  '     [length]: 0,\n' +
  '     [name]: \'func\',\n' +
  '     [prototype]: func { [constructor]: [Circular] } } }');
assert.strictEqual(
  util.format('%o', nestedObj2),
  '{ foo: \'bar\',\n' +
  '  foobar: 1,\n' +
  '  func:\n' +
  '   [ { a:\n' +
  '        { [Function: a]\n' +
  '          [length]: 0,\n' +
  '          [name]: \'a\',\n' +
  '          [prototype]: a { [constructor]: [Circular] } } },\n' +
  '     [length]: 1 ] }');
assert.strictEqual(
  util.format('%o', nestedObj),
  '{ foo: \'bar\',\n' +
  '  foobar:\n' +
  '   { foo: \'bar\',\n' +
  '     func:\n' +
  '      { [Function: func]\n' +
  '        [length]: 0,\n' +
  '        [name]: \'func\',\n' +
  '        [prototype]: func { [constructor]: [Circular] } } } }');
assert.strictEqual(
  util.format('%o %o', obj, obj),
  '{ foo: \'bar\',\n' +
  '  foobar: 1,\n' +
  '  func:\n' +
  '   { [Function: func]\n' +
  '     [length]: 0,\n' +
  '     [name]: \'func\',\n' +
  '     [prototype]: func { [constructor]: [Circular] } } }' +
  ' { foo: \'bar\',\n' +
  '  foobar: 1,\n' +
  '  func:\n' +
  '   { [Function: func]\n' +
  '     [length]: 0,\n' +
  '     [name]: \'func\',\n' +
  '     [prototype]: func { [constructor]: [Circular] } } }');
assert.strictEqual(
  util.format('%o %o', obj),
  '{ foo: \'bar\',\n' +
  '  foobar: 1,\n' +
  '  func:\n' +
  '   { [Function: func]\n' +
  '     [length]: 0,\n' +
  '     [name]: \'func\',\n' +
  '     [prototype]: func { [constructor]: [Circular] } } } %o');

assert.strictEqual(util.format('%O'), '%O');
assert.strictEqual(util.format('%O', 42), '42');
assert.strictEqual(util.format('%O', 'foo'), '\'foo\'');
assert.strictEqual(
  util.format('%O', obj),
  '{ foo: \'bar\', foobar: 1, func: [Function: func] }');
assert.strictEqual(
  util.format('%O', nestedObj),
  '{ foo: \'bar\', foobar: { foo: \'bar\', func: [Function: func] } }');
assert.strictEqual(
  util.format('%O %O', obj, obj),
  '{ foo: \'bar\', foobar: 1, func: [Function: func] } ' +
  '{ foo: \'bar\', foobar: 1, func: [Function: func] }');
assert.strictEqual(
  util.format('%O %O', obj),
  '{ foo: \'bar\', foobar: 1, func: [Function: func] } %O');

// Various format specifiers
assert.strictEqual(util.format('%%s%s', 'foo'), '%sfoo');
assert.strictEqual(util.format('%s:%s'), '%s:%s');
assert.strictEqual(util.format('%s:%s', undefined), 'undefined:%s');
assert.strictEqual(util.format('%s:%s', 'foo'), 'foo:%s');
assert.strictEqual(util.format('%s:%i', 'foo'), 'foo:%i');
assert.strictEqual(util.format('%s:%f', 'foo'), 'foo:%f');
assert.strictEqual(util.format('%s:%s', 'foo', 'bar'), 'foo:bar');
assert.strictEqual(util.format('%s:%s', 'foo', 'bar', 'baz'), 'foo:bar baz');
assert.strictEqual(util.format('%%%s%%', 'hi'), '%hi%');
assert.strictEqual(util.format('%%%s%%%%', 'hi'), '%hi%%');
assert.strictEqual(util.format('%sbc%%def', 'a'), 'abc%def');
assert.strictEqual(util.format('%d:%d', 12, 30), '12:30');
assert.strictEqual(util.format('%d:%d', 12), '12:%d');
assert.strictEqual(util.format('%d:%d'), '%d:%d');
assert.strictEqual(util.format('%i:%i', 12, 30), '12:30');
assert.strictEqual(util.format('%i:%i', 12), '12:%i');
assert.strictEqual(util.format('%i:%i'), '%i:%i');
assert.strictEqual(util.format('%f:%f', 12, 30), '12:30');
assert.strictEqual(util.format('%f:%f', 12), '12:%f');
assert.strictEqual(util.format('%f:%f'), '%f:%f');
assert.strictEqual(util.format('o: %j, a: %j', {}, []), 'o: {}, a: []');
assert.strictEqual(util.format('o: %j, a: %j', {}), 'o: {}, a: %j');
assert.strictEqual(util.format('o: %j, a: %j'), 'o: %j, a: %j');
assert.strictEqual(util.format('o: %o, a: %O', {}, []), 'o: {}, a: []');
assert.strictEqual(util.format('o: %o, a: %o', {}), 'o: {}, a: %o');
assert.strictEqual(util.format('o: %O, a: %O'), 'o: %O, a: %O');


// Invalid format specifiers
assert.strictEqual(util.format('a% b', 'x'), 'a% b x');
assert.strictEqual(util.format('percent: %d%, fraction: %d', 10, 0.1),
                   'percent: 10%, fraction: 0.1');
assert.strictEqual(util.format('abc%', 1), 'abc% 1');

// Additional arguments after format specifiers
assert.strictEqual(util.format('%i', 1, 'number'), '1 number');
assert.strictEqual(util.format('%i', 1, () => {}), '1 [Function]');

{
  const o = {};
  o.o = o;
  assert.strictEqual(util.format('%j', o), '[Circular]');
}

{
  const o = {
    toJSON() {
      throw new Error('Not a circular object but still not serializable');
    }
  };
  assert.throws(() => util.format('%j', o),
                /^Error: Not a circular object but still not serializable$/);
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
Object.setPrototypeOf(BadCustomError.prototype, Error.prototype);
Object.setPrototypeOf(BadCustomError, Error);
assert.strictEqual(util.format(new BadCustomError('foo')),
                   '[BadCustomError: foo]');

// The format of arguments should not depend on type of the first argument
assert.strictEqual(util.format('1', '1'), '1 1');
assert.strictEqual(util.format(1, '1'), '1 1');
assert.strictEqual(util.format('1', 1), '1 1');
assert.strictEqual(util.format(1, -0), '1 -0');
assert.strictEqual(util.format('1', () => {}), '1 [Function]');
assert.strictEqual(util.format(1, () => {}), '1 [Function]');
assert.strictEqual(util.format('1', "'"), "1 '");
assert.strictEqual(util.format(1, "'"), "1 '");
assert.strictEqual(util.format('1', 'number'), '1 number');
assert.strictEqual(util.format(1, 'number'), '1 number');
assert.strictEqual(util.format(5n), '5n');
assert.strictEqual(util.format(5n, 5n), '5n 5n');

// Check `formatWithOptions`.
assert.strictEqual(
  util.formatWithOptions(
    { colors: true },
    true, undefined, Symbol(), 1, 5n, null, 'foobar'
  ),
  '\u001b[33mtrue\u001b[39m ' +
    '\u001b[90mundefined\u001b[39m ' +
    '\u001b[32mSymbol()\u001b[39m ' +
    '\u001b[33m1\u001b[39m ' +
    '\u001b[33m5n\u001b[39m ' +
    '\u001b[1mnull\u001b[22m ' +
    'foobar'
);

assert.strictEqual(
  util.format(new SharedArrayBuffer(4)),
  'SharedArrayBuffer { [Uint8Contents]: <00 00 00 00>, byteLength: 4 }'
);
