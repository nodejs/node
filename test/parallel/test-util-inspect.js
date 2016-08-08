'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const vm = require('vm');

assert.equal(util.inspect(1), '1');
assert.equal(util.inspect(false), 'false');
assert.equal(util.inspect(''), "''");
assert.equal(util.inspect('hello'), "'hello'");
assert.equal(util.inspect(function() {}), '[Function]');
assert.equal(util.inspect(undefined), 'undefined');
assert.equal(util.inspect(null), 'null');
assert.equal(util.inspect(/foo(bar\r\n)?/gi), '/foo(bar\\r\\n)?/gi');
assert.strictEqual(util.inspect(new Date('Sun, 14 Feb 2010 11:48:40 GMT')),
  new Date('2010-02-14T12:48:40+01:00').toISOString());
assert.strictEqual(util.inspect(new Date('')), (new Date('')).toString());

assert.equal(util.inspect('\r\n\u0001'), "'\\r\\n\\u0001'");

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

assert(/Object/.test(
  util.inspect({a: {a: {a: {a: {}}}}}, undefined, undefined, true)
));
assert(!/Object/.test(
  util.inspect({a: {a: {a: {a: {}}}}}, undefined, null, true)
));

for (const showHidden of [true, false]) {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab, 1, 2);
  assert.equal(util.inspect(ab, showHidden), 'ArrayBuffer { byteLength: 4 }');
  assert.equal(util.inspect(new DataView(ab, 1, 2), showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4 } }');
  assert.equal(util.inspect(ab, showHidden), 'ArrayBuffer { byteLength: 4 }');
  assert.equal(util.inspect(dv, showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.equal(util.inspect(ab, showHidden),
               'ArrayBuffer { byteLength: 4, x: 42 }');
  assert.equal(util.inspect(dv, showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4, x: 42 },\r\n' +
               '  y: 1337 }');
}

// Now do the same checks but from a different context
for (const showHidden of [true, false]) {
  const ab = vm.runInNewContext('new ArrayBuffer(4)');
  const dv = vm.runInNewContext('new DataView(ab, 1, 2)', { ab: ab });
  assert.equal(util.inspect(ab, showHidden), 'ArrayBuffer { byteLength: 4 }');
  assert.equal(util.inspect(new DataView(ab, 1, 2), showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4 } }');
  assert.equal(util.inspect(ab, showHidden), 'ArrayBuffer { byteLength: 4 }');
  assert.equal(util.inspect(dv, showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.equal(util.inspect(ab, showHidden),
               'ArrayBuffer { byteLength: 4, x: 42 }');
  assert.equal(util.inspect(dv, showHidden),
               'DataView {\r\n' +
               '  byteLength: 2,\r\n' +
               '  byteOffset: 1,\r\n' +
               '  buffer: ArrayBuffer { byteLength: 4, x: 42 },\r\n' +
               '  y: 1337 }');
}


[ Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray ].forEach((constructor) => {
    const length = 2;
    const byteLength = length * constructor.BYTES_PER_ELEMENT;
    const array = new constructor(new ArrayBuffer(byteLength), 0, length);
    array[0] = 65;
    array[1] = 97;
    assert.equal(util.inspect(array, true),
                 `${constructor.name} [\r\n` +
                 `  65,\r\n` +
                 `  97,\r\n` +
                 `  [BYTES_PER_ELEMENT]: ${constructor.BYTES_PER_ELEMENT}` +
                 `,\r\n  [length]: ${length},\r\n` +
                 `  [byteLength]: ${byteLength},\r\n` +
                 `  [byteOffset]: 0,\r\n` +
                 `  [buffer]: ArrayBuffer { byteLength: ${byteLength} } ]`);
    assert.equal(util.inspect(array, false), `${constructor.name} [ 65, 97 ]`);
  });

// Now check that declaring a TypedArray in a different context works the same
[ Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray ].forEach((constructor) => {
    const length = 2;
    const byteLength = length * constructor.BYTES_PER_ELEMENT;
    const array = vm.runInNewContext('new constructor(new ArrayBuffer(' +
                                     'byteLength), 0, length)',
                                     { constructor: constructor,
                                       byteLength: byteLength,
                                       length: length
                                     });
    array[0] = 65;
    array[1] = 97;
    assert.equal(util.inspect(array, true),
                 `${constructor.name} [\r\n` +
                 `  65,\r\n` +
                 `  97,\r\n` +
                 `  [BYTES_PER_ELEMENT]: ${constructor.BYTES_PER_ELEMENT}` +
                 `,\r\n  [length]: ${length},\r\n` +
                 `  [byteLength]: ${byteLength},\r\n` +
                 `  [byteOffset]: 0,\r\n` +
                 `  [buffer]: ArrayBuffer { byteLength: ${byteLength} } ]`);
    assert.equal(util.inspect(array, false), `${constructor.name} [ 65, 97 ]`);
  });

// Due to the hash seed randomization it's not deterministic the order that
// the following ways this hash is displayed.
// See http://codereview.chromium.org/9124004/

{
  const out = util.inspect(Object.create({},
      {visible: {value: 1, enumerable: true}, hidden: {value: 2}}), true);
  if (out !== '{ [hidden]: 2, visible: 1 }' &&
      out !== '{ visible: 1, [hidden]: 2 }') {
    assert.ok(false);
  }
}

// Objects without prototype
{
  const out = util.inspect(Object.create(null,
      { name: {value: 'Tim', enumerable: true},
        hidden: {value: 'secret'}}), true);
  if (out !== "{ [hidden]: 'secret', name: 'Tim' }" &&
      out !== "{ name: 'Tim', [hidden]: 'secret' }") {
    assert(false);
  }
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
Object.defineProperty(
  value,
  'growingLength',
  {
    enumerable: true,
    get: () => { this.push(true); return this.length; }
  }
);
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
assert.equal(util.inspect(value), '{ 2010-02-14T11:48:40.000Z aprop: 42 }'
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
{
  const Debug = require('vm').runInDebugContext('Debug');
  const map = new Map();
  map.set(1, 2);
  const mirror = Debug.MakeMirror(map.entries(), true);
  const vals = mirror.preview();
  const valsOutput = [];
  for (const o of vals) {
    valsOutput.push(o);
  }

  assert.strictEqual(util.inspect(valsOutput), '[ [ 1, 2 ] ]');
}

// test for other constructors in different context
var obj = require('vm').runInNewContext('(function(){return {}})()', {});
assert.strictEqual(util.inspect(obj), '{}');
obj = require('vm').runInNewContext('var m=new Map();m.set(1,2);m', {});
assert.strictEqual(util.inspect(obj), 'Map { 1 => 2 }');
obj = require('vm').runInNewContext('var s=new Set();s.add(1);s.add(2);s', {});
assert.strictEqual(util.inspect(obj), 'Set { 1, 2 }');
obj = require('vm').runInNewContext('fn=function(){};new Promise(fn,fn)', {});
assert.strictEqual(util.inspect(obj), 'Promise { <pending> }');

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
const errors = [];
errors.push(new Error());
errors.push(new Error('FAIL'));
errors.push(new TypeError('FAIL'));
errors.push(new SyntaxError('FAIL'));
errors.forEach(function(err) {
  assert.equal(util.inspect(err), err.stack);
});
try {
  undef(); // eslint-disable-line no-undef
} catch (e) {
  assert.equal(util.inspect(e), e.stack);
}
var ex = util.inspect(new Error('FAIL'), true);
assert.ok(ex.indexOf('Error: FAIL') != -1);
assert.ok(ex.indexOf('[stack]') != -1);
assert.ok(ex.indexOf('[message]') != -1);
// Doesn't capture stack trace
function BadCustomError(msg) {
  Error.call(this);
  Object.defineProperty(this, 'message',
                        { value: msg, enumerable: false });
  Object.defineProperty(this, 'name',
                        { value: 'BadCustomError', enumerable: false });
}
util.inherits(BadCustomError, Error);
assert.equal(util.inspect(new BadCustomError('foo')), '[BadCustomError: foo]');

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
  var d = new Date();
  d.toISOString = null;
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
{
  const x = { inspect: util.inspect };
  assert.ok(util.inspect(x).indexOf('inspect') != -1);
}

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
{
  let subject = { foo: 'bar', hello: 31, a: { b: { c: { d: 0 } } } };
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
}

// util.inspect with "colors" option should produce as many lines as without it
function test_lines(input) {
  var count_lines = function(str) {
    return (str.match(/\r\n/g) || []).length;
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
assert.equal(util.inspect(Object(Symbol('test'))), '[Symbol: Symbol(test)]');
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

  const options = { showHidden: true };
  let subject = {};

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
{
  assert.equal(util.inspect(new Map()), 'Map {}');
  assert.equal(util.inspect(new Map([[1, 'a'], [2, 'b'], [3, 'c']])),
               'Map { 1 => \'a\', 2 => \'b\', 3 => \'c\' }');
  const map = new Map([['foo', null]]);
  map.bar = 42;
  assert.equal(util.inspect(map, true),
               'Map { \'foo\' => null, [size]: 1, bar: 42 }');
}

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
  var lines = util.inspect(container).split('\r\n');
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
{
  class ObjectSubclass {}
  class ArraySubclass extends Array {}
  class SetSubclass extends Set {}
  class MapSubclass extends Map {}
  class PromiseSubclass extends Promise {}

  const x = new ObjectSubclass();
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
}

// Corner cases.
{
  const x = { constructor: 42 };
  assert.equal(util.inspect(x), '{ constructor: 42 }');
}

{
  const x = {};
  Object.defineProperty(x, 'constructor', {
    get: function() {
      throw new Error('should not access constructor');
    },
    enumerable: true
  });
  assert.equal(util.inspect(x), '{ constructor: [Getter] }');
}

{
  const x = new function() {};
  assert.equal(util.inspect(x), '{}');
}

{
  const x = Object.create(null);
  assert.equal(util.inspect(x), '{}');
}

// The following maxArrayLength tests were introduced after v6.0.0 was released.
// Do not backport to v5/v4 unless all of
// https://github.com/nodejs/node/pull/6334 is backported.
{
  const x = Array(101);
  assert(/1 more item/.test(util.inspect(x)));
}

{
  const x = Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: 101})));
}

{
  const x = Array(101);
  assert(/^\[ ... 101 more items \]$/.test(
      util.inspect(x, {maxArrayLength: 0})));
}

{
  const x = new Uint8Array(101);
  assert(/1 more item/.test(util.inspect(x)));
}

{
  const x = new Uint8Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: 101})));
}

{
  const x = new Uint8Array(101);
  assert(/\[ ... 101 more items \]$/.test(
      util.inspect(x, {maxArrayLength: 0})));
}

{
  const x = Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: null})));
}

{
  const x = Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: Infinity})));
}

{
  const x = new Uint8Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: null})));
}

{
  const x = new Uint8Array(101);
  assert(!/1 more item/.test(util.inspect(x, {maxArrayLength: Infinity})));
}

{
  const obj = {foo: 'abc', bar: 'xyz'};
  const oneLine = util.inspect(obj, {breakLength: Infinity});
  // Subtract four for the object's two curly braces and two spaces of padding.
  // Add one more to satisfy the strictly greater than condition in the code.
  const breakpoint = oneLine.length - 5;
  const twoLines = util.inspect(obj, {breakLength: breakpoint});

  assert.strictEqual(oneLine, '{ foo: \'abc\', bar: \'xyz\' }');
  assert.strictEqual(oneLine, util.inspect(obj, {breakLength: breakpoint + 1}));
  assert.strictEqual(twoLines, '{ foo: \'abc\',\r\n  bar: \'xyz\' }');
}

// util.inspect.defaultOptions tests
{
  const arr = Array(101);
  const obj = {a: {a: {a: {a: 1}}}};

  const oldOptions = Object.assign({}, util.inspect.defaultOptions);

  // Set single option through property assignment
  util.inspect.defaultOptions.maxArrayLength = null;
  assert(!/1 more item/.test(util.inspect(arr)));
  util.inspect.defaultOptions.maxArrayLength = oldOptions.maxArrayLength;
  assert(/1 more item/.test(util.inspect(arr)));
  util.inspect.defaultOptions.depth = null;
  assert(!/Object/.test(util.inspect(obj)));
  util.inspect.defaultOptions.depth = oldOptions.depth;
  assert(/Object/.test(util.inspect(obj)));
  assert.strictEqual(
    JSON.stringify(util.inspect.defaultOptions),
    JSON.stringify(oldOptions)
  );

  // Set multiple options through object assignment
  util.inspect.defaultOptions = {maxArrayLength: null, depth: null};
  assert(!/1 more item/.test(util.inspect(arr)));
  assert(!/Object/.test(util.inspect(obj)));
  util.inspect.defaultOptions = oldOptions;
  assert(/1 more item/.test(util.inspect(arr)));
  assert(/Object/.test(util.inspect(obj)));
  assert.strictEqual(
    JSON.stringify(util.inspect.defaultOptions),
    JSON.stringify(oldOptions)
  );
}
