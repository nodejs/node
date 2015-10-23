'use strict';
var common = require('../common');
var assert = require('assert');
var util = require('util');

assert.equal(util.inspect(1), '1');
assert.equal(util.inspect(false), 'false');
assert.equal(util.inspect(''), "''");
assert.equal(util.inspect('hello'), "'hello'");
assert.equal(util.inspect(function() {}), '[Function]');
assert.equal(util.inspect(undefined), 'undefined');
assert.equal(util.inspect(null), 'null');
assert.equal(util.inspect(/foo(bar\n)?/gi), '/foo(bar\\n)?/gi');
assert.equal(util.inspect(new Date('Sun, 14 Feb 2010 11:48:40 GMT')),
  new Date('2010-02-14T12:48:40+01:00').toString());

assert.equal(util.inspect('\n\u0001'), "'\\n\\u0001'");

assert.equal(util.inspect([]), '[]');
assert.equal(util.inspect(Object.create([])), 'Array {}');
assert.equal(util.inspect([1, 2]), '[ 1, 2 ]');
assert.equal(util.inspect([1, [2, 3]]), '[ 1, [ 2, 3 ] ]');

assert.equal(util.inspect({}), '{}');
assert.equal(util.inspect({a: 1}), '{ a: 1 }');
assert.equal(util.inspect({a: function() {}}), '{ a: [Function] }');
assert.equal(util.inspect({a: 1, b: 2}), '{ a: 1, b: 2 }');
assert.equal(util.inspect({'a': {}}), '{ a: {} }');
assert.equal(util.inspect({'a': {'b': 2}}), '{ a: { b: 2 } }');
assert.equal(util.inspect({'a': {'b': { 'c': { 'd': 2 }}}}),
  '{ a: { b: { c: [Object] } } }');
assert.equal(util.inspect({'a': {'b': { 'c': { 'd': 2 }}}}, false, null),
  '{ a: { b: { c: { d: 2 } } } }');
assert.equal(util.inspect([1, 2, 3], true), '[ 1, 2, 3, [length]: 3 ]');
assert.equal(util.inspect({'a': {'b': { 'c': 2}}}, false, 0),
  '{ a: [Object] }');
assert.equal(util.inspect({'a': {'b': { 'c': 2}}}, false, 1),
  '{ a: { b: [Object] } }');
assert.equal(util.inspect(Object.create({},
  {visible: {value: 1, enumerable: true}, hidden: {value: 2}})),
  '{ visible: 1 }'
);

// Due to the hash seed randomization it's not deterministic the order that
// the following ways this hash is displayed.
// See http://codereview.chromium.org/9124004/

var out = util.inspect(Object.create({},
    {visible: {value: 1, enumerable: true}, hidden: {value: 2}}), true);
if (out !== '{ [hidden]: 2, visible: 1 }' &&
    out !== '{ visible: 1, [hidden]: 2 }') {
  assert.ok(false);
}


// Objects without prototype
var out = util.inspect(Object.create(null,
    { name: {value: 'Tim', enumerable: true},
      hidden: {value: 'secret'}}), true);
if (out !== "{ [hidden]: 'secret', name: 'Tim' }" &&
    out !== "{ name: 'Tim', [hidden]: 'secret' }") {
  assert(false);
}


assert.equal(
  util.inspect(Object.create(null,
    {name: {value: 'Tim', enumerable: true},
      hidden: {value: 'secret'}})),
  '{ name: \'Tim\' }'
);


// Dynamic properties
assert.equal(util.inspect({get readonly() {}}),
  '{ readonly: [Getter] }');

assert.equal(util.inspect({get readwrite() {}, set readwrite(val) {}}),
  '{ readwrite: [Getter/Setter] }');

assert.equal(util.inspect({set writeonly(val) {}}),
  '{ writeonly: [Setter] }');

var value = {};
value['a'] = value;
assert.equal(util.inspect(value), '{ a: [Circular] }');

// Array with dynamic properties
value = [1, 2, 3];
value.__defineGetter__('growingLength', function() {
  this.push(true); return this.length;
});
assert.equal(util.inspect(value), '[ 1, 2, 3, growingLength: [Getter] ]');

// Function with properties
value = function() {};
value.aprop = 42;
assert.equal(util.inspect(value), '{ [Function] aprop: 42 }');

// Regular expressions with properties
value = /123/ig;
value.aprop = 42;
assert.equal(util.inspect(value), '{ /123/gi aprop: 42 }');

// Dates with properties
value = new Date('Sun, 14 Feb 2010 11:48:40 GMT');
value.aprop = 42;
assert.equal(util.inspect(value), '{ Sun, 14 Feb 2010 11:48:40 GMT aprop: 42 }'
);

// test the internal isDate implementation
var Date2 = require('vm').runInNewContext('Date');
var d = new Date2();
var orig = util.inspect(d);
Date2.prototype.foo = 'bar';
var after = util.inspect(d);
assert.equal(orig, after);

// test positive/negative zero
assert.equal(util.inspect(0), '0');
assert.equal(util.inspect(-0), '-0');

// test for sparse array
var a = ['foo', 'bar', 'baz'];
assert.equal(util.inspect(a), '[ \'foo\', \'bar\', \'baz\' ]');
delete a[1];
assert.equal(util.inspect(a), '[ \'foo\', , \'baz\' ]');
assert.equal(util.inspect(a, true), '[ \'foo\', , \'baz\', [length]: 3 ]');
assert.equal(util.inspect(new Array(5)), '[ , , , ,  ]');

// test for Array constructor in different context
const Debug = require('vm').runInDebugContext('Debug');
var map = new Map();
map.set(1, 2);
var mirror = Debug.MakeMirror(map.entries(), true);
var vals = mirror.preview();
var valsOutput = [];
for (let o of vals) {
  valsOutput.push(o);
}

assert.strictEqual(util.inspect(valsOutput), '[ [ 1, 2 ] ]');

// test for property descriptors
var getter = Object.create(null, {
  a: {
    get: function() { return 'aaa'; }
  }
});
var setter = Object.create(null, {
  b: {
    set: function() {}
  }
});
var getterAndSetter = Object.create(null, {
  c: {
    get: function() { return 'ccc'; },
    set: function() {}
  }
});
assert.equal(util.inspect(getter, true), '{ [a]: [Getter] }');
assert.equal(util.inspect(setter, true), '{ [b]: [Setter] }');
assert.equal(util.inspect(getterAndSetter, true), '{ [c]: [Getter/Setter] }');

// exceptions should print the error message, not '{}'
assert.equal(util.inspect(new Error()), '[Error]');
assert.equal(util.inspect(new Error('FAIL')), '[Error: FAIL]');
assert.equal(util.inspect(new TypeError('FAIL')), '[TypeError: FAIL]');
assert.equal(util.inspect(new SyntaxError('FAIL')), '[SyntaxError: FAIL]');
try {
  undef();
} catch (e) {
  assert.equal(util.inspect(e), '[ReferenceError: undef is not defined]');
}
var ex = util.inspect(new Error('FAIL'), true);
assert.ok(ex.indexOf('[Error: FAIL]') != -1);
assert.ok(ex.indexOf('[stack]') != -1);
assert.ok(ex.indexOf('[message]') != -1);

// GH-1941
// should not throw:
assert.equal(util.inspect(Object.create(Date.prototype)), 'Date {}');

// GH-1944
assert.doesNotThrow(function() {
  var d = new Date();
  d.toUTCString = null;
  util.inspect(d);
});

assert.doesNotThrow(function() {
  var r = /regexp/;
  r.toString = null;
  util.inspect(r);
});

// bug with user-supplied inspect function returns non-string
assert.doesNotThrow(function() {
  util.inspect([{
    inspect: function() { return 123; }
  }]);
});

// GH-2225
var x = { inspect: util.inspect };
assert.ok(util.inspect(x).indexOf('inspect') != -1);

// util.inspect should not display the escaped value of a key.
var w = {
  '\\': 1,
  '\\\\': 2,
  '\\\\\\': 3,
  '\\\\\\\\': 4,
};

var y = ['a', 'b', 'c'];
y['\\\\\\'] = 'd';

assert.ok(util.inspect(w),
          '{ \'\\\': 1, \'\\\\\': 2, \'\\\\\\\': 3, \'\\\\\\\\\': 4 }');
assert.ok(util.inspect(y), '[ \'a\', \'b\', \'c\', \'\\\\\\\': \'d\' ]');

// util.inspect.styles and util.inspect.colors
function test_color_style(style, input, implicit) {
  var color_name = util.inspect.styles[style];
  var color = ['', ''];
  if (util.inspect.colors[color_name])
    color = util.inspect.colors[color_name];

  var without_color = util.inspect(input, false, 0, false);
  var with_color = util.inspect(input, false, 0, true);
  var expect = '\u001b[' + color[0] + 'm' + without_color +
               '\u001b[' + color[1] + 'm';
  assert.equal(with_color, expect, 'util.inspect color for style ' + style);
}

test_color_style('special', function() {});
test_color_style('number', 123.456);
test_color_style('boolean', true);
test_color_style('undefined', undefined);
test_color_style('null', null);
test_color_style('string', 'test string');
test_color_style('date', new Date());
test_color_style('regexp', /regexp/);

// an object with "hasOwnProperty" overwritten should not throw
assert.doesNotThrow(function() {
  util.inspect({
    hasOwnProperty: null
  });
});

// new API, accepts an "options" object
var subject = { foo: 'bar', hello: 31, a: { b: { c: { d: 0 } } } };
Object.defineProperty(subject, 'hidden', { enumerable: false, value: null });

assert(util.inspect(subject, { showHidden: false }).indexOf('hidden') === -1);
assert(util.inspect(subject, { showHidden: true }).indexOf('hidden') !== -1);
assert(util.inspect(subject, { colors: false }).indexOf('\u001b[32m') === -1);
assert(util.inspect(subject, { colors: true }).indexOf('\u001b[32m') !== -1);
assert(util.inspect(subject, { depth: 2 }).indexOf('c: [Object]') !== -1);
assert(util.inspect(subject, { depth: 0 }).indexOf('a: [Object]') !== -1);
assert(util.inspect(subject, { depth: null }).indexOf('{ d: 0 }') !== -1);

// "customInspect" option can enable/disable calling inspect() on objects
subject = { inspect: function() { return 123; } };

assert(util.inspect(subject,
                    { customInspect: true }).indexOf('123') !== -1);
assert(util.inspect(subject,
                    { customInspect: true }).indexOf('inspect') === -1);
assert(util.inspect(subject,
                    { customInspect: false }).indexOf('123') === -1);
assert(util.inspect(subject,
                    { customInspect: false }).indexOf('inspect') !== -1);

// custom inspect() functions should be able to return other Objects
subject.inspect = function() { return { foo: 'bar' }; };

assert.equal(util.inspect(subject), '{ foo: \'bar\' }');

subject.inspect = function(depth, opts) {
  assert.strictEqual(opts.customInspectOptions, true);
};

util.inspect(subject, { customInspectOptions: true });

// util.inspect with "colors" option should produce as many lines as without it
function test_lines(input) {
  var count_lines = function(str) {
    return (str.match(/\n/g) || []).length;
  };

  var without_color = util.inspect(input);
  var with_color = util.inspect(input, {colors: true});
  assert.equal(count_lines(without_color), count_lines(with_color));
}

test_lines([1, 2, 3, 4, 5, 6, 7]);
test_lines(function() {
  var big_array = [];
  for (var i = 0; i < 100; i++) {
    big_array.push(i);
  }
  return big_array;
}());
test_lines({foo: 'bar', baz: 35, b: {a: 35}});
test_lines({
  foo: 'bar',
  baz: 35,
  b: {a: 35},
  very_long_key: 'very_long_value',
  even_longer_key: ['with even longer value in array']
});

// test boxed primitives output the correct values
assert.equal(util.inspect(new String('test')), '[String: \'test\']');
assert.equal(util.inspect(new Boolean(false)), '[Boolean: false]');
assert.equal(util.inspect(new Boolean(true)), '[Boolean: true]');
assert.equal(util.inspect(new Number(0)), '[Number: 0]');
assert.equal(util.inspect(new Number(-0)), '[Number: -0]');
assert.equal(util.inspect(new Number(-1.1)), '[Number: -1.1]');
assert.equal(util.inspect(new Number(13.37)), '[Number: 13.37]');

// test boxed primitives with own properties
var str = new String('baz');
str.foo = 'bar';
assert.equal(util.inspect(str), '{ [String: \'baz\'] foo: \'bar\' }');

var bool = new Boolean(true);
bool.foo = 'bar';
assert.equal(util.inspect(bool), '{ [Boolean: true] foo: \'bar\' }');

var num = new Number(13.37);
num.foo = 'bar';
assert.equal(util.inspect(num), '{ [Number: 13.37] foo: \'bar\' }');

// test es6 Symbol
if (typeof Symbol !== 'undefined') {
  assert.equal(util.inspect(Symbol()), 'Symbol()');
  assert.equal(util.inspect(Symbol(123)), 'Symbol(123)');
  assert.equal(util.inspect(Symbol('hi')), 'Symbol(hi)');
  assert.equal(util.inspect([Symbol()]), '[ Symbol() ]');
  assert.equal(util.inspect({ foo: Symbol() }), '{ foo: Symbol() }');

  var options = { showHidden: true };
  var subject = {};

  subject[Symbol('symbol')] = 42;

  assert.equal(util.inspect(subject), '{}');
  assert.equal(util.inspect(subject, options), '{ [Symbol(symbol)]: 42 }');

  subject = [1, 2, 3];
  subject[Symbol('symbol')] = 42;

  assert.equal(util.inspect(subject), '[ 1, 2, 3 ]');
  assert.equal(util.inspect(subject, options),
      '[ 1, 2, 3, [length]: 3, [Symbol(symbol)]: 42 ]');

}

// test Set
assert.equal(util.inspect(new Set()), 'Set {}');
assert.equal(util.inspect(new Set([1, 2, 3])), 'Set { 1, 2, 3 }');
var set = new Set(['foo']);
set.bar = 42;
assert.equal(util.inspect(set, true), 'Set { \'foo\', [size]: 1, bar: 42 }');

// test Map
assert.equal(util.inspect(new Map()), 'Map {}');
assert.equal(util.inspect(new Map([[1, 'a'], [2, 'b'], [3, 'c']])),
             'Map { 1 => \'a\', 2 => \'b\', 3 => \'c\' }');
var map = new Map([['foo', null]]);
map.bar = 42;
assert.equal(util.inspect(map, true),
             'Map { \'foo\' => null, [size]: 1, bar: 42 }');

// test Promise
assert.equal(util.inspect(Promise.resolve(3)), 'Promise { 3 }');
assert.equal(util.inspect(Promise.reject(3)), 'Promise { <rejected> 3 }');
assert.equal(util.inspect(new Promise(function() {})), 'Promise { <pending> }');
var promise = Promise.resolve('foo');
promise.bar = 42;
assert.equal(util.inspect(promise), 'Promise { \'foo\', bar: 42 }');

// Make sure it doesn't choke on polyfills. Unlike Set/Map, there is no standard
// interface to synchronously inspect a Promise, so our techniques only work on
// a bonafide native Promise.
var oldPromise = Promise;
global.Promise = function() { this.bar = 42; };
assert.equal(util.inspect(new Promise()), '{ bar: 42 }');
global.Promise = oldPromise;

// Map/Set Iterators
var m = new Map([['foo', 'bar']]);
assert.strictEqual(util.inspect(m.keys()), 'MapIterator { \'foo\' }');
assert.strictEqual(util.inspect(m.values()), 'MapIterator { \'bar\' }');
assert.strictEqual(util.inspect(m.entries()),
                   'MapIterator { [ \'foo\', \'bar\' ] }');
// make sure the iterator doesn't get consumed
var keys = m.keys();
assert.strictEqual(util.inspect(keys), 'MapIterator { \'foo\' }');
assert.strictEqual(util.inspect(keys), 'MapIterator { \'foo\' }');

var s = new Set([1, 3]);
assert.strictEqual(util.inspect(s.keys()), 'SetIterator { 1, 3 }');
assert.strictEqual(util.inspect(s.values()), 'SetIterator { 1, 3 }');
assert.strictEqual(util.inspect(s.entries()),
                   'SetIterator { [ 1, 1 ], [ 3, 3 ] }');
// make sure the iterator doesn't get consumed
keys = s.keys();
assert.strictEqual(util.inspect(keys), 'SetIterator { 1, 3 }');
assert.strictEqual(util.inspect(keys), 'SetIterator { 1, 3 }');

// Test alignment of items in container
// Assumes that the first numeric character is the start of an item.

function checkAlignment(container) {
  var lines = util.inspect(container).split('\n');
  var pos;
  lines.forEach(function(line) {
    var npos = line.search(/\d/);
    if (npos !== -1) {
      if (pos !== undefined)
        assert.equal(pos, npos, 'container items not aligned');
      pos = npos;
    }
  });
}

var big_array = [];
for (var i = 0; i < 100; i++) {
  big_array.push(i);
}

checkAlignment(big_array);
checkAlignment(function() {
  var obj = {};
  big_array.forEach(function(v) {
    obj[v] = null;
  });
  return obj;
}());
checkAlignment(new Set(big_array));
checkAlignment(new Map(big_array.map(function(y) { return [y, null]; })));


// Test display of constructors

class ObjectSubclass {}
class ArraySubclass extends Array {}
class SetSubclass extends Set {}
class MapSubclass extends Map {}
class PromiseSubclass extends Promise {}

var x = new ObjectSubclass();
x.foo = 42;
assert.equal(util.inspect(x),
             'ObjectSubclass { foo: 42 }');
assert.equal(util.inspect(new ArraySubclass(1, 2, 3)),
             'ArraySubclass [ 1, 2, 3 ]');
assert.equal(util.inspect(new SetSubclass([1, 2, 3])),
             'SetSubclass { 1, 2, 3 }');
assert.equal(util.inspect(new MapSubclass([['foo', 42]])),
            'MapSubclass { \'foo\' => 42 }');
assert.equal(util.inspect(new PromiseSubclass(function() {})),
             'PromiseSubclass { <pending> }');

// Corner cases.
var x = { constructor: 42 };
assert.equal(util.inspect(x), '{ constructor: 42 }');

var x = {};
Object.defineProperty(x, 'constructor', {
  get: function() {
    throw new Error('should not access constructor');
  },
  enumerable: true
});
assert.equal(util.inspect(x), '{ constructor: [Getter] }');

var x = new (function() {});
assert.equal(util.inspect(x), '{}');

var x = Object.create(null);
assert.equal(util.inspect(x), '{}');
