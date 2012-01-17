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

var common = require('../common');
var assert = require('assert');
var a = require('assert');

function makeBlock(f) {
  var args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return f.apply(this, args);
  };
}

assert.ok(common.indirectInstanceOf(a.AssertionError.prototype, Error),
          'a.AssertionError instanceof Error');

assert.throws(makeBlock(a, false), a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a, true), a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a, 'test', 'ok(\'test\')'));

assert.throws(makeBlock(a.ok, false),
              a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a.ok, true),
                    a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a.ok, 'test'), 'ok(\'test\')');

assert.throws(makeBlock(a.equal, true, false), a.AssertionError, 'equal');

assert.doesNotThrow(makeBlock(a.equal, null, null), 'equal');

assert.doesNotThrow(makeBlock(a.equal, undefined, undefined), 'equal');

assert.doesNotThrow(makeBlock(a.equal, null, undefined), 'equal');

assert.doesNotThrow(makeBlock(a.equal, true, true), 'equal');

assert.doesNotThrow(makeBlock(a.equal, 2, '2'), 'equal');

assert.doesNotThrow(makeBlock(a.notEqual, true, false), 'notEqual');

assert.throws(makeBlock(a.notEqual, true, true),
              a.AssertionError, 'notEqual');

assert.throws(makeBlock(a.strictEqual, 2, '2'),
              a.AssertionError, 'strictEqual');

assert.throws(makeBlock(a.strictEqual, null, undefined),
              a.AssertionError, 'strictEqual');

assert.doesNotThrow(makeBlock(a.notStrictEqual, 2, '2'), 'notStrictEqual');

// deepEquals joy!
// 7.2
assert.doesNotThrow(makeBlock(a.deepEqual, new Date(2000, 3, 14),
                    new Date(2000, 3, 14)), 'deepEqual date');

assert.throws(makeBlock(a.deepEqual, new Date(), new Date(2000, 3, 14)),
              a.AssertionError,
              'deepEqual date');

// 7.3
assert.doesNotThrow(makeBlock(a.deepEqual, 4, '4'), 'deepEqual == check');
assert.doesNotThrow(makeBlock(a.deepEqual, true, 1), 'deepEqual == check');
assert.throws(makeBlock(a.deepEqual, 4, '5'),
              a.AssertionError,
              'deepEqual == check');

// 7.4
// having the same number of owned properties && the same set of keys
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4}, {a: 4}));
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4, b: '2'}, {a: 4, b: '2'}));
assert.doesNotThrow(makeBlock(a.deepEqual, [4], ['4']));
assert.throws(makeBlock(a.deepEqual, {a: 4}, {a: 4, b: true}),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, ['a'], {0: 'a'}));
//(although not necessarily the same order),
assert.doesNotThrow(makeBlock(a.deepEqual, {a: 4, b: '1'}, {b: '1', a: 4}));
var a1 = [1, 2, 3];
var a2 = [1, 2, 3];
a1.a = 'test';
a1.b = true;
a2.b = true;
a2.a = 'test';
assert.throws(makeBlock(a.deepEqual, Object.keys(a1), Object.keys(a2)),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, a1, a2));

// having an identical prototype property
var nbRoot = {
  toString: function() { return this.first + ' ' + this.last; }
};

function nameBuilder(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder.prototype = nbRoot;

function nameBuilder2(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder2.prototype = nbRoot;

var nb1 = new nameBuilder('Ryan', 'Dahl');
var nb2 = new nameBuilder2('Ryan', 'Dahl');

assert.doesNotThrow(makeBlock(a.deepEqual, nb1, nb2));

nameBuilder2.prototype = Object;
nb2 = new nameBuilder2('Ryan', 'Dahl');
assert.throws(makeBlock(a.deepEqual, nb1, nb2), a.AssertionError);

// String literal + object blew up my implementation...
assert.throws(makeBlock(a.deepEqual, 'a', {}), a.AssertionError);

// Testing the throwing
function thrower(errorConstructor) {
  throw new errorConstructor('test');
}
var aethrow = makeBlock(thrower, a.AssertionError);
aethrow = makeBlock(thrower, a.AssertionError);

// the basic calls work
assert.throws(makeBlock(thrower, a.AssertionError),
              a.AssertionError, 'message');
assert.throws(makeBlock(thrower, a.AssertionError), a.AssertionError);
assert.throws(makeBlock(thrower, a.AssertionError));

// if not passing an error, catch all.
assert.throws(makeBlock(thrower, TypeError));

// when passing a type, only catch errors of the appropriate type
var threw = false;
try {
  a.throws(makeBlock(thrower, TypeError), a.AssertionError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof TypeError, 'type');
}
assert.equal(true, threw,
             'a.throws with an explicit error is eating extra errors',
             a.AssertionError);
threw = false;

// doesNotThrow should pass through all errors
try {
  a.doesNotThrow(makeBlock(thrower, TypeError), a.AssertionError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof TypeError);
}
assert.equal(true, threw,
             'a.doesNotThrow with an explicit error is eating extra errors');

// key difference is that throwing our correct error makes an assertion error
try {
  a.doesNotThrow(makeBlock(thrower, TypeError), TypeError);
} catch (e) {
  threw = true;
  assert.ok(e instanceof a.AssertionError);
}
assert.equal(true, threw,
             'a.doesNotThrow is not catching type matching errors');

assert.throws(function() {assert.ifError(new Error('test error'))});
assert.doesNotThrow(function() {assert.ifError(null)});
assert.doesNotThrow(function() {assert.ifError()});

// make sure that validating using constructor really works
threw = false;
try {
  assert.throws(
      function() {
        throw {};
      },
      Array
  );
} catch (e) {
  threw = true;
}
assert.ok(threw, 'wrong constructor validation');

// use a RegExp to validate error message
a.throws(makeBlock(thrower, TypeError), /test/);

// use a fn to validate error object
a.throws(makeBlock(thrower, TypeError), function(err) {
  if ((err instanceof TypeError) && /test/.test(err)) {
    return true;
  }
});


// GH-207. Make sure deepEqual doesn't loop forever on circular refs

var b = {};
b.b = b;

var c = {};
c.b = c;

var gotError = false;
try {
  assert.deepEqual(b, c);
} catch (e) {
  gotError = true;
}

console.log('All OK');
assert.ok(gotError);


// #217
function testAssertionMessage(actual, expected) {
  try {
    assert.equal(actual, '');
  } catch (e) {
    assert.equal(e.toString(),
        ['AssertionError:', expected, '==', '""'].join(' '));
  }
}
testAssertionMessage(undefined, '"undefined"');
testAssertionMessage(null, 'null');
testAssertionMessage(true, 'true');
testAssertionMessage(false, 'false');
testAssertionMessage(0, '0');
testAssertionMessage(100, '100');
testAssertionMessage(NaN, '"NaN"');
testAssertionMessage(Infinity, '"Infinity"');
testAssertionMessage(-Infinity, '"-Infinity"');
testAssertionMessage('', '""');
testAssertionMessage('foo', '"foo"');
testAssertionMessage([], '[]');
testAssertionMessage([1, 2, 3], '[1,2,3]');
testAssertionMessage(/a/, '"/a/"');
testAssertionMessage(/abc/gim, '"/abc/gim"');
testAssertionMessage(function f() {}, '"function f() {}"');
testAssertionMessage({}, '{}');
testAssertionMessage({a: undefined, b: null}, '{"a":"undefined","b":null}');
testAssertionMessage({a: NaN, b: Infinity, c: -Infinity},
    '{"a":"NaN","b":"Infinity","c":"-Infinity"}');

