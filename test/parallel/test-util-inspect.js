// Flags: --expose-internals
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
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const JSStream = process.binding('js_stream').JSStream;
const util = require('util');
const vm = require('vm');
const { previewEntries } = internalBinding('util');
const { inspect } = util;

assert.strictEqual(util.inspect(1), '1');
assert.strictEqual(util.inspect(false), 'false');
assert.strictEqual(util.inspect(''), "''");
assert.strictEqual(util.inspect('hello'), "'hello'");
assert.strictEqual(util.inspect(function() {}), '[Function]');
assert.strictEqual(util.inspect(() => {}), '[Function]');
assert.strictEqual(util.inspect(async function() {}), '[AsyncFunction]');
assert.strictEqual(util.inspect(async () => {}), '[AsyncFunction]');
assert.strictEqual(util.inspect(function*() {}), '[GeneratorFunction]');
assert.strictEqual(util.inspect(undefined), 'undefined');
assert.strictEqual(util.inspect(null), 'null');
assert.strictEqual(util.inspect(/foo(bar\n)?/gi), '/foo(bar\\n)?/gi');
assert.strictEqual(
  util.inspect(new Date('Sun, 14 Feb 2010 11:48:40 GMT')),
  new Date('2010-02-14T12:48:40+01:00').toISOString()
);
assert.strictEqual(util.inspect(new Date('')), (new Date('')).toString());
assert.strictEqual(util.inspect('\n\u0001'), "'\\n\\u0001'");
assert.strictEqual(
  util.inspect(`${Array(75).fill(1)}'\n\u001d\n\u0003`),
  `"${Array(75).fill(1)}'\\n\\u001d\\n\\u0003"`
);
assert.strictEqual(util.inspect([]), '[]');
assert.strictEqual(util.inspect(Object.create([])), 'Array {}');
assert.strictEqual(util.inspect([1, 2]), '[ 1, 2 ]');
assert.strictEqual(util.inspect([1, [2, 3]]), '[ 1, [ 2, 3 ] ]');
assert.strictEqual(util.inspect({}), '{}');
assert.strictEqual(util.inspect({ a: 1 }), '{ a: 1 }');
assert.strictEqual(util.inspect({ a: function() {} }), '{ a: [Function: a] }');
assert.strictEqual(util.inspect({ a: () => {} }), '{ a: [Function: a] }');
assert.strictEqual(util.inspect({ a: async function() {} }),
                   '{ a: [AsyncFunction: a] }');
assert.strictEqual(util.inspect({ a: async () => {} }),
                   '{ a: [AsyncFunction: a] }');
assert.strictEqual(util.inspect({ a: function*() {} }),
                   '{ a: [GeneratorFunction: a] }');
assert.strictEqual(util.inspect({ a: 1, b: 2 }), '{ a: 1, b: 2 }');
assert.strictEqual(util.inspect({ 'a': {} }), '{ a: {} }');
assert.strictEqual(util.inspect({ 'a': { 'b': 2 } }), '{ a: { b: 2 } }');
assert.strictEqual(util.inspect({ 'a': { 'b': { 'c': { 'd': 2 } } } }),
                   '{ a: { b: { c: [Object] } } }');
assert.strictEqual(
  util.inspect({ 'a': { 'b': { 'c': { 'd': 2 } } } }, false, null),
  '{ a: { b: { c: { d: 2 } } } }');
assert.strictEqual(util.inspect([1, 2, 3], true), '[ 1, 2, 3, [length]: 3 ]');
assert.strictEqual(util.inspect({ 'a': { 'b': { 'c': 2 } } }, false, 0),
                   '{ a: [Object] }');
assert.strictEqual(util.inspect({ 'a': { 'b': { 'c': 2 } } }, false, 1),
                   '{ a: { b: [Object] } }');
assert.strictEqual(util.inspect({ 'a': { 'b': ['c'] } }, false, 1),
                   '{ a: { b: [Array] } }');
assert.strictEqual(util.inspect(new Uint8Array(0)), 'Uint8Array []');
assert(inspect(new Uint8Array(0), { showHidden: true }).includes('[buffer]'));
assert.strictEqual(
  util.inspect(
    Object.create(
      {},
      { visible: { value: 1, enumerable: true }, hidden: { value: 2 } }
    )
  ),
  '{ visible: 1 }'
);
assert.strictEqual(
  util.inspect(
    Object.assign(new String('hello'), { [Symbol('foo')]: 123 }),
    { showHidden: true }
  ),
  "{ [String: 'hello'] [length]: 5, [Symbol(foo)]: 123 }"
);

assert.strictEqual(util.inspect((new JSStream())._externalStream),
                   '[External]');

{
  const regexp = /regexp/;
  regexp.aprop = 42;
  assert.strictEqual(util.inspect({ a: regexp }, false, 0), '{ a: /regexp/ }');
}

assert(/Object/.test(
  util.inspect({ a: { a: { a: { a: {} } } } }, undefined, undefined, true)
));
assert(!/Object/.test(
  util.inspect({ a: { a: { a: { a: {} } } } }, undefined, null, true)
));

{
  const showHidden = true;
  const ab = new Uint8Array([1, 2, 3, 4]).buffer;
  const dv = new DataView(ab, 1, 2);
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { [Uint8Contents]: <01 02 03 04>, byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(new DataView(ab, 1, 2), showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: ' +
                       '<01 02 03 04>, byteLength: 4 } }');
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { [Uint8Contents]: <01 02 03 04>, byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: ' +
                       '<01 02 03 04>, byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.strictEqual(util.inspect(ab, showHidden),
                     'ArrayBuffer { [Uint8Contents]: <01 02 03 04>, ' +
                       'byteLength: 4, x: 42 }');
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: <01 02 03 04>, ' +
                       'byteLength: 4, x: 42 },\n' +
                     '  y: 1337 }');
}

// Now do the same checks but from a different context.
{
  const showHidden = false;
  const ab = vm.runInNewContext('new ArrayBuffer(4)');
  const dv = vm.runInNewContext('new DataView(ab, 1, 2)', { ab });
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { [Uint8Contents]: <00 00 00 00>, byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(new DataView(ab, 1, 2), showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: <00 00 00 00>, ' +
                       'byteLength: 4 } }');
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { [Uint8Contents]: <00 00 00 00>, byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: <00 00 00 00>, ' +
                       'byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.strictEqual(util.inspect(ab, showHidden),
                     'ArrayBuffer { [Uint8Contents]: <00 00 00 00>, ' +
                       'byteLength: 4, x: 42 }');
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer:\n' +
                     '   ArrayBuffer { [Uint8Contents]: <00 00 00 00>,' +
                       ' byteLength: 4, x: 42 },\n' +
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
  assert.strictEqual(
    util.inspect(array, { showHidden: true }),
    `${constructor.name} [\n` +
      '  65,\n' +
      '  97,\n' +
      `  [BYTES_PER_ELEMENT]: ${constructor.BYTES_PER_ELEMENT},\n` +
      `  [length]: ${length},\n` +
      `  [byteLength]: ${byteLength},\n` +
      '  [byteOffset]: 0,\n' +
      `  [buffer]: ArrayBuffer { byteLength: ${byteLength} } ]`);
  assert.strictEqual(
    util.inspect(array, false),
    `${constructor.name} [ 65, 97 ]`
  );
});

// Now check that declaring a TypedArray in a different context works the same.
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
  const array = vm.runInNewContext(
    'new constructor(new ArrayBuffer(byteLength), 0, length)',
    { constructor, byteLength, length }
  );
  array[0] = 65;
  array[1] = 97;
  assert.strictEqual(
    util.inspect(array, true),
    `${constructor.name} [\n` +
      '  65,\n' +
      '  97,\n' +
      `  [BYTES_PER_ELEMENT]: ${constructor.BYTES_PER_ELEMENT},\n` +
      `  [length]: ${length},\n` +
      `  [byteLength]: ${byteLength},\n` +
      '  [byteOffset]: 0,\n' +
      `  [buffer]: ArrayBuffer { byteLength: ${byteLength} } ]`);
  assert.strictEqual(
    util.inspect(array, false),
    `${constructor.name} [ 65, 97 ]`
  );
});

assert.strictEqual(
  util.inspect(Object.create({}, {
    visible: { value: 1, enumerable: true },
    hidden: { value: 2 }
  }), { showHidden: true }),
  '{ visible: 1, [hidden]: 2 }'
);
// Objects without prototype.
assert.strictEqual(
  util.inspect(Object.create(null, {
    name: { value: 'Tim', enumerable: true },
    hidden: { value: 'secret' }
  }), { showHidden: true }),
  "[Object: null prototype] { name: 'Tim', [hidden]: 'secret' }"
);

assert.strictEqual(
  util.inspect(Object.create(null, {
    name: { value: 'Tim', enumerable: true },
    hidden: { value: 'secret' }
  })),
  "[Object: null prototype] { name: 'Tim' }"
);

// Dynamic properties.
{
  assert.strictEqual(
    util.inspect({ get readonly() {} }),
    '{ readonly: [Getter] }');

  assert.strictEqual(
    util.inspect({ get readwrite() {}, set readwrite(val) {} }),
    '{ readwrite: [Getter/Setter] }');

  assert.strictEqual(
    // eslint-disable-next-line accessor-pairs
    util.inspect({ set writeonly(val) {} }),
    '{ writeonly: [Setter] }');

  const value = {};
  value.a = value;
  assert.strictEqual(util.inspect(value), '{ a: [Circular] }');
}

// Array with dynamic properties.
{
  const value = [1, 2, 3];
  Object.defineProperty(
    value,
    'growingLength',
    {
      enumerable: true,
      get: function() { this.push(true); return this.length; }
    }
  );
  Object.defineProperty(
    value,
    '-1',
    {
      enumerable: true,
      value: -1
    }
  );
  assert.strictEqual(util.inspect(value),
                     "[ 1, 2, 3, growingLength: [Getter], '-1': -1 ]");
}

// Array with inherited number properties.
{
  class CustomArray extends Array {}
  CustomArray.prototype[5] = 'foo';
  const arr = new CustomArray(50);
  assert.strictEqual(util.inspect(arr), 'CustomArray [ <50 empty items> ]');
}

// Array with extra properties.
{
  const arr = [1, 2, 3, , ];
  arr.foo = 'bar';
  assert.strictEqual(util.inspect(arr),
                     "[ 1, 2, 3, <1 empty item>, foo: 'bar' ]");

  const arr2 = [];
  assert.strictEqual(util.inspect([], { showHidden: true }), '[ [length]: 0 ]');
  arr2['00'] = 1;
  assert.strictEqual(util.inspect(arr2), "[ '00': 1 ]");
  assert.strictEqual(util.inspect(arr2, { showHidden: true }),
                     "[ [length]: 0, '00': 1 ]");
  arr2[1] = 0;
  assert.strictEqual(util.inspect(arr2), "[ <1 empty item>, 0, '00': 1 ]");
  assert.strictEqual(util.inspect(arr2, { showHidden: true }),
                     "[ <1 empty item>, 0, [length]: 2, '00': 1 ]");
  delete arr2[1];
  assert.strictEqual(util.inspect(arr2), "[ <2 empty items>, '00': 1 ]");
  assert.strictEqual(util.inspect(arr2, { showHidden: true }),
                     "[ <2 empty items>, [length]: 2, '00': 1 ]");
  arr2['01'] = 2;
  assert.strictEqual(util.inspect(arr2),
                     "[ <2 empty items>, '00': 1, '01': 2 ]");
  assert.strictEqual(util.inspect(arr2, { showHidden: true }),
                     "[ <2 empty items>, [length]: 2, '00': 1, '01': 2 ]");
  delete arr2['00'];
  arr2[0] = 0;
  assert.strictEqual(util.inspect(arr2),
                     "[ 0, <1 empty item>, '01': 2 ]");
  assert.strictEqual(util.inspect(arr2, { showHidden: true }),
                     "[ 0, <1 empty item>, [length]: 2, '01': 2 ]");
  delete arr2['01'];
  arr2[2 ** 32 - 2] = 'max';
  arr2[2 ** 32 - 1] = 'too far';
  assert.strictEqual(
    util.inspect(arr2),
    "[ 0, <4294967293 empty items>, 'max', '4294967295': 'too far' ]"
  );

  const arr3 = [];
  arr3[-1] = -1;
  assert.strictEqual(util.inspect(arr3), "[ '-1': -1 ]");
}

// Indices out of bounds.
{
  const arr = [];
  arr[2 ** 32] = true; // Not a valid array index.
  assert.strictEqual(util.inspect(arr), "[ '4294967296': true ]");
  arr[0] = true;
  arr[10] = true;
  assert.strictEqual(util.inspect(arr),
                     "[ true, <9 empty items>, true, '4294967296': true ]");
  arr[2 ** 32 - 2] = true;
  arr[2 ** 32 - 1] = true;
  arr[2 ** 32 + 1] = true;
  delete arr[0];
  delete arr[10];
  assert.strictEqual(util.inspect(arr),
                     ['[ <4294967294 empty items>,',
                      'true,',
                      "'4294967296': true,",
                      "'4294967295': true,",
                      "'4294967297': true ]"
                     ].join('\n  '));
}

// Function with properties.
{
  const value = () => {};
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ [Function: value] aprop: 42 }');
}

// Anonymous function with properties.
{
  const value = (() => function() {})();
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ [Function] aprop: 42 }');
}

// Regular expressions with properties.
{
  const value = /123/ig;
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ /123/gi aprop: 42 }');
}

// Dates with properties.
{
  const value = new Date('Sun, 14 Feb 2010 11:48:40 GMT');
  value.aprop = 42;
  assert.strictEqual(util.inspect(value),
                     '{ 2010-02-14T11:48:40.000Z aprop: 42 }');
}

// Test the internal isDate implementation.
{
  const Date2 = vm.runInNewContext('Date');
  const d = new Date2();
  const orig = util.inspect(d);
  Date2.prototype.foo = 'bar';
  const after = util.inspect(d);
  assert.strictEqual(orig, after);
}

// Test positive/negative zero.
assert.strictEqual(util.inspect(0), '0');
assert.strictEqual(util.inspect(-0), '-0');
// Edge case from check.
assert.strictEqual(util.inspect(-5e-324), '-5e-324');

// Test for sparse array.
{
  const a = ['foo', 'bar', 'baz'];
  assert.strictEqual(util.inspect(a), "[ 'foo', 'bar', 'baz' ]");
  delete a[1];
  assert.strictEqual(util.inspect(a), "[ 'foo', <1 empty item>, 'baz' ]");
  assert.strictEqual(
    util.inspect(a, true),
    "[ 'foo', <1 empty item>, 'baz', [length]: 3 ]"
  );
  assert.strictEqual(util.inspect(new Array(5)), '[ <5 empty items> ]');
  a[3] = 'bar';
  a[100] = 'qux';
  assert.strictEqual(
    util.inspect(a, { breakLength: Infinity }),
    "[ 'foo', <1 empty item>, 'baz', 'bar', <96 empty items>, 'qux' ]"
  );
  delete a[3];
  assert.strictEqual(
    util.inspect(a, { maxArrayLength: 4 }),
    "[ 'foo', <1 empty item>, 'baz', <97 empty items>, ... 1 more item ]"
  );
}

// Test for Array constructor in different context.
{
  const map = new Map();
  map.set(1, 2);
  // Passing only a single argument to indicate a set iterator.
  const valsSetIterator = previewEntries(map.entries());
  // Passing through true to indicate a map iterator.
  const valsMapIterEntries = previewEntries(map.entries(), true);
  const valsMapIterKeys = previewEntries(map.keys(), true);

  assert.strictEqual(util.inspect(valsSetIterator), '[ 1, 2 ]');
  assert.strictEqual(util.inspect(valsMapIterEntries), '[ [ 1, 2 ], true ]');
  assert.strictEqual(util.inspect(valsMapIterKeys), '[ [ 1 ], false ]');
}

// Test for other constructors in different context.
{
  let obj = vm.runInNewContext('(function(){return {}})()', {});
  assert.strictEqual(util.inspect(obj), '{}');
  obj = vm.runInNewContext('var m=new Map();m.set(1,2);m', {});
  assert.strictEqual(util.inspect(obj), 'Map { 1 => 2 }');
  obj = vm.runInNewContext('var s=new Set();s.add(1);s.add(2);s', {});
  assert.strictEqual(util.inspect(obj), 'Set { 1, 2 }');
  obj = vm.runInNewContext('fn=function(){};new Promise(fn,fn)', {});
  assert.strictEqual(util.inspect(obj), 'Promise { <pending> }');
}

// Test for property descriptors.
{
  const getter = Object.create(null, {
    a: {
      get: function() { return 'aaa'; }
    }
  });
  const setter = Object.create(null, {
    b: { // eslint-disable-line accessor-pairs
      set: function() {}
    }
  });
  const getterAndSetter = Object.create(null, {
    c: {
      get: function() { return 'ccc'; },
      set: function() {}
    }
  });
  assert.strictEqual(
    util.inspect(getter, true),
    '[Object: null prototype] { [a]: [Getter] }'
  );
  assert.strictEqual(
    util.inspect(setter, true),
    '[Object: null prototype] { [b]: [Setter] }'
  );
  assert.strictEqual(
    util.inspect(getterAndSetter, true),
    '[Object: null prototype] { [c]: [Getter/Setter] }'
  );
}

// Exceptions should print the error message, not '{}'.
{
  [
    new Error(),
    new Error('FAIL'),
    new TypeError('FAIL'),
    new SyntaxError('FAIL')
  ].forEach((err) => {
    assert.strictEqual(util.inspect(err), err.stack);
  });
  try {
    undef(); // eslint-disable-line no-undef
  } catch (e) {
    assert.strictEqual(util.inspect(e), e.stack);
  }
  const ex = util.inspect(new Error('FAIL'), true);
  assert(ex.includes('Error: FAIL'));
  assert(ex.includes('[stack]'));
  assert(ex.includes('[message]'));
}

{
  const tmp = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  const err = new Error('foo');
  const err2 = new Error('foo\nbar');
  assert.strictEqual(util.inspect(err, { compact: true }), '[Error: foo]');
  assert(err.stack);
  delete err.stack;
  assert(!err.stack);
  assert.strictEqual(util.inspect(err, { compact: true }), '[Error: foo]');
  assert.strictEqual(
    util.inspect(err2, { compact: true }),
    '[Error: foo\nbar]'
  );

  err.bar = true;
  err2.bar = true;

  assert.strictEqual(
    util.inspect(err, { compact: true }),
    '{ [Error: foo] bar: true }'
  );
  assert.strictEqual(
    util.inspect(err2, { compact: true }),
    '{ [Error: foo\nbar] bar: true }'
  );
  assert.strictEqual(
    util.inspect(err, { compact: true, breakLength: 5 }),
    '{ [Error: foo]\n  bar: true }'
  );
  assert.strictEqual(
    util.inspect(err, { compact: true, breakLength: 1 }),
    '{ [Error: foo]\n  bar:\n   true }'
  );
  assert.strictEqual(
    util.inspect(err2, { compact: true, breakLength: 5 }),
    '{ [Error: foo\nbar]\n  bar: true }'
  );
  assert.strictEqual(
    util.inspect(err, { compact: false }),
    '[Error: foo] {\n  bar: true\n}'
  );
  assert.strictEqual(
    util.inspect(err2, { compact: false }),
    '[Error: foo\nbar] {\n  bar: true\n}'
  );

  Error.stackTraceLimit = tmp;
}

// Doesn't capture stack trace.
{
  function BadCustomError(msg) {
    Error.call(this);
    Object.defineProperty(this, 'message',
                          { value: msg, enumerable: false });
    Object.defineProperty(this, 'name',
                          { value: 'BadCustomError', enumerable: false });
  }
  Object.setPrototypeOf(BadCustomError.prototype, Error.prototype);
  Object.setPrototypeOf(BadCustomError, Error);
  assert.strictEqual(
    util.inspect(new BadCustomError('foo')),
    '[BadCustomError: foo]'
  );
}

// https://github.com/nodejs/node-v0.x-archive/issues/1941
assert.strictEqual(util.inspect(Object.create(Date.prototype)), 'Date {}');

// https://github.com/nodejs/node-v0.x-archive/issues/1944
{
  const d = new Date();
  d.toUTCString = null;
  util.inspect(d);
}

// Should not throw.
{
  const d = new Date();
  d.toISOString = null;
  util.inspect(d);
}

// Should not throw.
{
  const r = /regexp/;
  r.toString = null;
  util.inspect(r);
}

// See https://github.com/nodejs/node-v0.x-archive/issues/2225
{
  const x = { [util.inspect.custom]: util.inspect };
  assert(util.inspect(x).includes(
    '[Symbol(nodejs.util.inspect.custom)]:\n   { [Function: inspect]'));
}

// `util.inspect` should display the escaped value of a key.
{
  const w = {
    '\\': 1,
    '\\\\': 2,
    '\\\\\\': 3,
    '\\\\\\\\': 4,
    '\n': 5,
    '\r': 6
  };

  const y = ['a', 'b', 'c'];
  y['\\\\'] = 'd';
  y['\n'] = 'e';
  y['\r'] = 'f';

  assert.strictEqual(
    util.inspect(w),
    "{ '\\\\': 1, '\\\\\\\\': 2, '\\\\\\\\\\\\': 3, " +
    "'\\\\\\\\\\\\\\\\': 4, '\\n': 5, '\\r': 6 }"
  );
  assert.strictEqual(
    util.inspect(y),
    "[ 'a', 'b', 'c', '\\\\\\\\': 'd', " +
    "'\\n': 'e', '\\r': 'f' ]"
  );
}

// Test util.inspect.styles and util.inspect.colors.
{
  function testColorStyle(style, input, implicit) {
    const colorName = util.inspect.styles[style];
    let color = ['', ''];
    if (util.inspect.colors[colorName])
      color = util.inspect.colors[colorName];

    const withoutColor = util.inspect(input, false, 0, false);
    const withColor = util.inspect(input, false, 0, true);
    const expect = `\u001b[${color[0]}m${withoutColor}\u001b[${color[1]}m`;
    assert.strictEqual(
      withColor,
      expect,
      `util.inspect color for style ${style}`);
  }

  testColorStyle('special', function() {});
  testColorStyle('number', 123.456);
  testColorStyle('boolean', true);
  testColorStyle('undefined', undefined);
  testColorStyle('null', null);
  testColorStyle('string', 'test string');
  testColorStyle('date', new Date());
  testColorStyle('regexp', /regexp/);
}

// An object with "hasOwnProperty" overwritten should not throw.
util.inspect({ hasOwnProperty: null });

// New API, accepts an "options" object.
{
  const subject = { foo: 'bar', hello: 31, a: { b: { c: { d: 0 } } } };
  Object.defineProperty(subject, 'hidden', { enumerable: false, value: null });

  assert.strictEqual(
    util.inspect(subject, { showHidden: false }).includes('hidden'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { showHidden: true }).includes('hidden'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { colors: false }).includes('\u001b[32m'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { colors: true }).includes('\u001b[32m'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { depth: 2 }).includes('c: [Object]'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { depth: 0 }).includes('a: [Object]'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { depth: null }).includes('{ d: 0 }'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { depth: undefined }).includes('{ d: 0 }'),
    true
  );
}

{
  // "customInspect" option can enable/disable calling [util.inspect.custom]().
  const subject = { [util.inspect.custom]: () => 123 };

  assert.strictEqual(
    util.inspect(subject, { customInspect: true }).includes('123'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: true }).includes('inspect'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: false }).includes('123'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: false }).includes('inspect'),
    true
  );

  // A custom [util.inspect.custom]() should be able to return other Objects.
  subject[util.inspect.custom] = () => ({ foo: 'bar' });

  assert.strictEqual(util.inspect(subject), "{ foo: 'bar' }");

  subject[util.inspect.custom] = (depth, opts) => {
    assert.strictEqual(opts.customInspectOptions, true);
  };

  util.inspect(subject, { customInspectOptions: true });

  // util.inspect.custom is a shared symbol which can be accessed as
  // Symbol.for("nodejs.util.inspect.custom").
  const inspect = Symbol.for('nodejs.util.inspect.custom');

  subject[inspect] = () => ({ baz: 'quux' });

  assert.strictEqual(util.inspect(subject), '{ baz: \'quux\' }');

  subject[inspect] = (depth, opts) => {
    assert.strictEqual(opts.customInspectOptions, true);
  };

  util.inspect(subject, { customInspectOptions: true });
}

{
  // Returning `this` from a custom inspection function works.
  const subject = { a: 123, [util.inspect.custom]() { return this; } };
  const UIC = 'nodejs.util.inspect.custom';
  assert.strictEqual(util.inspect(subject),
                     `{ a: 123,\n  [Symbol(${UIC})]: [Function: [${UIC}]] }`);
}

// Using `util.inspect` with "colors" option should produce as many lines as
// without it.
{
  function testLines(input) {
    const countLines = (str) => (str.match(/\n/g) || []).length;
    const withoutColor = util.inspect(input);
    const withColor = util.inspect(input, { colors: true });
    assert.strictEqual(countLines(withoutColor), countLines(withColor));
  }

  const bigArray = new Array(100).fill().map((value, index) => index);

  testLines([1, 2, 3, 4, 5, 6, 7]);
  testLines(bigArray);
  testLines({ foo: 'bar', baz: 35, b: { a: 35 } });
  testLines({ a: { a: 3, b: 1, c: 1, d: 1, e: 1, f: 1, g: 1, h: 1 }, b: 1 });
  testLines({
    foo: 'bar',
    baz: 35,
    b: { a: 35 },
    veryLongKey: 'very long value',
    evenLongerKey: ['with even longer value in array']
  });
}

// Test boxed primitives output the correct values.
assert.strictEqual(util.inspect(new String('test')), "[String: 'test']");
assert.strictEqual(
  util.inspect(Object(Symbol('test'))),
  '[Symbol: Symbol(test)]'
);
assert.strictEqual(util.inspect(new Boolean(false)), '[Boolean: false]');
assert.strictEqual(util.inspect(new Boolean(true)), '[Boolean: true]');
assert.strictEqual(util.inspect(new Number(0)), '[Number: 0]');
assert.strictEqual(util.inspect(new Number(-0)), '[Number: -0]');
assert.strictEqual(util.inspect(new Number(-1.1)), '[Number: -1.1]');
assert.strictEqual(util.inspect(new Number(13.37)), '[Number: 13.37]');

// Test boxed primitives with own properties.
{
  const str = new String('baz');
  str.foo = 'bar';
  assert.strictEqual(util.inspect(str), "{ [String: 'baz'] foo: 'bar' }");

  const bool = new Boolean(true);
  bool.foo = 'bar';
  assert.strictEqual(util.inspect(bool), "{ [Boolean: true] foo: 'bar' }");

  const num = new Number(13.37);
  num.foo = 'bar';
  assert.strictEqual(util.inspect(num), "{ [Number: 13.37] foo: 'bar' }");

  const sym = Object(Symbol('foo'));
  sym.foo = 'bar';
  assert.strictEqual(util.inspect(sym), "{ [Symbol: Symbol(foo)] foo: 'bar' }");

  const big = Object(BigInt(55));
  big.foo = 'bar';
  assert.strictEqual(util.inspect(big), "{ [BigInt: 55n] foo: 'bar' }");
}

// Test es6 Symbol.
if (typeof Symbol !== 'undefined') {
  assert.strictEqual(util.inspect(Symbol()), 'Symbol()');
  assert.strictEqual(util.inspect(Symbol(123)), 'Symbol(123)');
  assert.strictEqual(util.inspect(Symbol('hi')), 'Symbol(hi)');
  assert.strictEqual(util.inspect([Symbol()]), '[ Symbol() ]');
  assert.strictEqual(util.inspect({ foo: Symbol() }), '{ foo: Symbol() }');

  const options = { showHidden: true };
  let subject = {};

  subject[Symbol('sym\nbol')] = 42;

  assert.strictEqual(util.inspect(subject), '{ [Symbol(sym\\nbol)]: 42 }');
  assert.strictEqual(
    util.inspect(subject, options),
    '{ [Symbol(sym\\nbol)]: 42 }'
  );

  Object.defineProperty(
    subject,
    Symbol(),
    { enumerable: false, value: 'non-enum' });
  assert.strictEqual(util.inspect(subject), '{ [Symbol(sym\\nbol)]: 42 }');
  assert.strictEqual(
    util.inspect(subject, options),
    "{ [Symbol(sym\\nbol)]: 42, [Symbol()]: 'non-enum' }"
  );

  subject = [1, 2, 3];
  subject[Symbol('symbol')] = 42;

  assert.strictEqual(util.inspect(subject),
                     '[ 1, 2, 3, [Symbol(symbol)]: 42 ]');
}

// Test Set.
{
  assert.strictEqual(util.inspect(new Set()), 'Set {}');
  assert.strictEqual(util.inspect(new Set([1, 2, 3])), 'Set { 1, 2, 3 }');
  const set = new Set(['foo']);
  set.bar = 42;
  assert.strictEqual(
    util.inspect(set, { showHidden: true }),
    "Set { 'foo', [size]: 1, bar: 42 }"
  );
}

// Test circular Set.
{
  const set = new Set();
  set.add(set);
  assert.strictEqual(util.inspect(set), 'Set { [Circular] }');
}

// Test Map.
{
  assert.strictEqual(util.inspect(new Map()), 'Map {}');
  assert.strictEqual(util.inspect(new Map([[1, 'a'], [2, 'b'], [3, 'c']])),
                     "Map { 1 => 'a', 2 => 'b', 3 => 'c' }");
  const map = new Map([['foo', null]]);
  map.bar = 42;
  assert.strictEqual(util.inspect(map, true),
                     "Map { 'foo' => null, [size]: 1, bar: 42 }");
}

// Test circular Map.
{
  const map = new Map();
  map.set(map, 'map');
  assert.strictEqual(util.inspect(map), "Map { [Circular] => 'map' }");
  map.set(map, map);
  assert.strictEqual(util.inspect(map), 'Map { [Circular] => [Circular] }');
  map.delete(map);
  map.set('map', map);
  assert.strictEqual(util.inspect(map), "Map { 'map' => [Circular] }");
}

// Test Promise.
{
  const resolved = Promise.resolve(3);
  assert.strictEqual(util.inspect(resolved), 'Promise { 3 }');

  const rejected = Promise.reject(3);
  assert.strictEqual(util.inspect(rejected), 'Promise { <rejected> 3 }');
  // Squelch UnhandledPromiseRejection.
  rejected.catch(() => {});

  const pending = new Promise(() => {});
  assert.strictEqual(util.inspect(pending), 'Promise { <pending> }');

  const promiseWithProperty = Promise.resolve('foo');
  promiseWithProperty.bar = 42;
  assert.strictEqual(util.inspect(promiseWithProperty),
                     "Promise { 'foo', bar: 42 }");
}

// Make sure it doesn't choke on polyfills. Unlike Set/Map, there is no standard
// interface to synchronously inspect a Promise, so our techniques only work on
// a bonafide native Promise.
{
  const oldPromise = Promise;
  global.Promise = function() { this.bar = 42; };
  assert.strictEqual(util.inspect(new Promise()), '{ bar: 42 }');
  global.Promise = oldPromise;
}

// Test Map iterators.
{
  const map = new Map([['foo', 'bar']]);
  assert.strictEqual(util.inspect(map.keys()), '[Map Iterator] { \'foo\' }');
  assert.strictEqual(util.inspect(map.values()), '[Map Iterator] { \'bar\' }');
  map.set('A', 'B!');
  assert.strictEqual(util.inspect(map.entries(), { maxArrayLength: 1 }),
                     "[Map Iterator] { [ 'foo', 'bar' ], ... 1 more item }");
  // Make sure the iterator doesn't get consumed.
  const keys = map.keys();
  assert.strictEqual(util.inspect(keys), "[Map Iterator] { 'foo', 'A' }");
  assert.strictEqual(util.inspect(keys), "[Map Iterator] { 'foo', 'A' }");
  keys.extra = true;
  assert.strictEqual(
    util.inspect(keys, { maxArrayLength: 0 }),
    '[Map Iterator] { ... 2 more items, extra: true }');
}

// Test Set iterators.
{
  const aSet = new Set([1, 3]);
  assert.strictEqual(util.inspect(aSet.keys()), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(aSet.values()), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(aSet.entries()), '[Set Iterator] { 1, 3 }');
  // Make sure the iterator doesn't get consumed.
  const keys = aSet.keys();
  assert.strictEqual(util.inspect(keys), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(keys), '[Set Iterator] { 1, 3 }');
  keys.extra = true;
  assert.strictEqual(
    util.inspect(keys, { maxArrayLength: 1 }),
    '[Set Iterator] { 1, ... 1 more item, extra: true }');
}

// Test alignment of items in container.
// Assumes that the first numeric character is the start of an item.
{
  function checkAlignment(container) {
    const lines = util.inspect(container).split('\n');
    const numRE = /\d/;
    let pos;
    lines.forEach((line) => {
      const npos = line.search(numRE);
      if (npos !== -1) {
        if (pos !== undefined) {
          assert.strictEqual(pos, npos);
        }
        pos = npos;
      }
    });
  }

  const bigArray = [];
  for (let i = 0; i < 100; i++) {
    bigArray.push(i);
  }

  const obj = {};
  bigArray.forEach((prop) => {
    obj[prop] = null;
  });

  checkAlignment(bigArray);
  checkAlignment(obj);
  checkAlignment(new Set(bigArray));
  checkAlignment(new Map(bigArray.map((number) => [number, null])));
}


// Test display of constructors.
{
  class ObjectSubclass {}
  class ArraySubclass extends Array {}
  class SetSubclass extends Set {}
  class MapSubclass extends Map {}
  class PromiseSubclass extends Promise {}

  const x = new ObjectSubclass();
  x.foo = 42;
  assert.strictEqual(util.inspect(x),
                     'ObjectSubclass { foo: 42 }');
  assert.strictEqual(util.inspect(new ArraySubclass(1, 2, 3)),
                     'ArraySubclass [ 1, 2, 3 ]');
  assert.strictEqual(util.inspect(new SetSubclass([1, 2, 3])),
                     'SetSubclass [Set] { 1, 2, 3 }');
  assert.strictEqual(util.inspect(new MapSubclass([['foo', 42]])),
                     "MapSubclass [Map] { 'foo' => 42 }");
  assert.strictEqual(util.inspect(new PromiseSubclass(() => {})),
                     'PromiseSubclass [Promise] { <pending> }');
  assert.strictEqual(
    util.inspect({ a: { b: new ArraySubclass([1, [2], 3]) } }, { depth: 1 }),
    '{ a: { b: [ArraySubclass] } }'
  );
}

// Empty and circular before depth.
{
  const arr = [[[[]]]];
  assert.strictEqual(util.inspect(arr), '[ [ [ [] ] ] ]');
  arr[0][0][0][0] = [];
  assert.strictEqual(util.inspect(arr), '[ [ [ [Array] ] ] ]');
  arr[0][0][0] = {};
  assert.strictEqual(util.inspect(arr), '[ [ [ {} ] ] ]');
  arr[0][0][0] = { a: 2 };
  assert.strictEqual(util.inspect(arr), '[ [ [ [Object] ] ] ]');
  arr[0][0][0] = arr;
  assert.strictEqual(util.inspect(arr), '[ [ [ [Circular] ] ] ]');
}

// Corner cases.
{
  const x = { constructor: 42 };
  assert.strictEqual(util.inspect(x), '{ constructor: 42 }');
}

{
  const x = {};
  Object.defineProperty(x, 'constructor', {
    get: function() {
      throw new Error('should not access constructor');
    },
    enumerable: true
  });
  assert.strictEqual(util.inspect(x), '{ constructor: [Getter] }');
}

{
  const x = new function() {}; // eslint-disable-line new-parens
  assert.strictEqual(util.inspect(x), '{}');
}

{
  const x = Object.create(null);
  assert.strictEqual(util.inspect(x), '[Object: null prototype] {}');
}

{
  const x = [];
  x[''] = 1;
  assert.strictEqual(util.inspect(x), "[ '': 1 ]");
}

// The following maxArrayLength tests were introduced after v6.0.0 was released.
// Do not backport to v5/v4 unless all of
// https://github.com/nodejs/node/pull/6334 is backported.
{
  const x = new Array(101).fill();
  assert(util.inspect(x).endsWith('1 more item ]'));
  assert(!util.inspect(x, { maxArrayLength: 101 }).endsWith('1 more item ]'));
  assert.strictEqual(
    util.inspect(x, { maxArrayLength: -1 }),
    '[ ... 101 more items ]'
  );
  assert.strictEqual(util.inspect(x, { maxArrayLength: 0 }),
                     '[ ... 101 more items ]');
}

{
  const x = Array(101);
  assert.strictEqual(util.inspect(x, { maxArrayLength: 0 }),
                     '[ ... 101 more items ]');
  assert(!util.inspect(x, { maxArrayLength: null }).endsWith('1 more item ]'));
  assert(!util.inspect(
    x, { maxArrayLength: Infinity }
  ).endsWith('1 more item ]'));
}

{
  const x = new Uint8Array(101);
  assert(util.inspect(x).endsWith('1 more item ]'));
  assert(!util.inspect(x, { maxArrayLength: 101 }).endsWith('1 more item ]'));
  assert.strictEqual(util.inspect(x, { maxArrayLength: 0 }),
                     'Uint8Array [ ... 101 more items ]');
  assert(!util.inspect(x, { maxArrayLength: null }).endsWith('1 more item ]'));
  assert(util.inspect(x, { maxArrayLength: Infinity }).endsWith('  0 ]'));
}

{
  const obj = { foo: 'abc', bar: 'xyz' };
  const oneLine = util.inspect(obj, { breakLength: Infinity });
  // Subtract four for the object's two curly braces and two spaces of padding.
  // Add one more to satisfy the strictly greater than condition in the code.
  const breakpoint = oneLine.length - 5;
  const twoLines = util.inspect(obj, { breakLength: breakpoint });

  assert.strictEqual(oneLine, "{ foo: 'abc', bar: 'xyz' }");
  assert.strictEqual(oneLine,
                     util.inspect(obj, { breakLength: breakpoint + 1 }));
  assert.strictEqual(twoLines, "{ foo: 'abc',\n  bar: 'xyz' }");
}

// util.inspect.defaultOptions tests.
{
  const arr = new Array(101).fill();
  const obj = { a: { a: { a: { a: 1 } } } };

  const oldOptions = Object.assign({}, util.inspect.defaultOptions);

  // Set single option through property assignment.
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

  // Set multiple options through object assignment.
  util.inspect.defaultOptions = { maxArrayLength: null, depth: 2 };
  assert(!/1 more item/.test(util.inspect(arr)));
  assert(/Object/.test(util.inspect(obj)));
  util.inspect.defaultOptions = oldOptions;
  assert(/1 more item/.test(util.inspect(arr)));
  assert(/Object/.test(util.inspect(obj)));
  assert.strictEqual(
    JSON.stringify(util.inspect.defaultOptions),
    JSON.stringify(oldOptions)
  );

  common.expectsError(() => {
    util.inspect.defaultOptions = null;
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type Object. ' +
             'Received type object'
  }
  );

  common.expectsError(() => {
    util.inspect.defaultOptions = 'bad';
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type Object. ' +
             'Received type string'
  }
  );
}

util.inspect(process);

// Setting custom inspect property to a non-function should do nothing.
{
  const obj = { [util.inspect.custom]: 'fhqwhgads' };
  assert.strictEqual(
    util.inspect(obj),
    "{ [Symbol(nodejs.util.inspect.custom)]: 'fhqwhgads' }"
  );
}

{
  // @@toStringTag
  assert.strictEqual(util.inspect({ [Symbol.toStringTag]: 'a' }),
                     "Object [a] { [Symbol(Symbol.toStringTag)]: 'a' }");

  class Foo {
    constructor() {
      this.foo = 'bar';
    }

    get [Symbol.toStringTag]() {
      return this.foo;
    }
  }

  assert.strictEqual(util.inspect(
    Object.create(null, { [Symbol.toStringTag]: { value: 'foo' } })),
                     '[Object: null prototype] [foo] {}');

  assert.strictEqual(util.inspect(new Foo()), "Foo [bar] { foo: 'bar' }");

  assert.strictEqual(
    util.inspect(new (class extends Foo {})()),
    "Foo [bar] { foo: 'bar' }");

  assert.strictEqual(
    util.inspect(Object.create(Object.create(Foo.prototype), {
      foo: { value: 'bar', enumerable: true }
    })),
    "Foo [bar] { foo: 'bar' }");

  class ThrowingClass {
    get [Symbol.toStringTag]() {
      throw new Error('toStringTag error');
    }
  }

  assert.throws(() => util.inspect(new ThrowingClass()), /toStringTag error/);

  class NotStringClass {
    get [Symbol.toStringTag]() {
      return null;
    }
  }

  assert.strictEqual(util.inspect(new NotStringClass()),
                     'NotStringClass {}');
}

{
  const o = {
    a: [1, 2, [[
      'Lorem ipsum dolor\nsit amet,\tconsectetur adipiscing elit, sed do ' +
        'eiusmod tempor incididunt ut labore et dolore magna aliqua.',
      'test',
      'foo']], 4],
    b: new Map([['za', 1], ['zb', 'test']])
  };

  let out = util.inspect(o, { compact: true, depth: 5, breakLength: 80 });
  let expect = [
    '{ a:',
    '   [ 1,',
    '     2,',
    "     [ [ 'Lorem ipsum dolor\\nsit amet,\\tconsectetur adipiscing elit, " +
      "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.',",
    "         'test',",
    "         'foo' ] ],",
    '     4 ],',
    "  b: Map { 'za' => 1, 'zb' => 'test' } }",
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(o, { compact: false, depth: 5, breakLength: 60 });
  expect = [
    '{',
    '  a: [',
    '    1,',
    '    2,',
    '    [',
    '      [',
    "        'Lorem ipsum dolor\\nsit amet,\\tconsectetur ' +",
    "          'adipiscing elit, sed do eiusmod tempor ' +",
    "          'incididunt ut labore et dolore magna ' +",
    "          'aliqua.',",
    "        'test',",
    "        'foo'",
    '      ]',
    '    ],',
    '    4',
    '  ],',
    '  b: Map {',
    "    'za' => 1,",
    "    'zb' => 'test'",
    '  }',
    '}'
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(o.a[2][0][0], { compact: false, breakLength: 30 });
  expect = [
    "'Lorem ipsum dolor\\nsit ' +",
    "  'amet,\\tconsectetur ' +",
    "  'adipiscing elit, sed do ' +",
    "  'eiusmod tempor incididunt ' +",
    "  'ut labore et dolore magna ' +",
    "  'aliqua.'"
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12345678901234567890123456789012345678901234567890',
    { compact: false, breakLength: 3 });
  expect = "'12345678901234567890123456789012345678901234567890'";
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 123456789012345678901234567890',
    { compact: false, breakLength: 3 });
  expect = [
    "'12 45 78 01 34 ' +",
    "  '67 90 23 56 89 ' +",
    "  '123456789012345678901234567890'"
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 1234567890123 0',
    { compact: false, breakLength: 3 });
  expect = [
    "'12 45 78 01 34 ' +",
    "  '67 90 23 56 89 ' +",
    "  '1234567890123 0'"
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 12345678901234567 0',
    { compact: false, breakLength: 3 });
  expect = [
    "'12 45 78 01 34 ' +",
    "  '67 90 23 56 89 ' +",
    "  '12345678901234567 ' +",
    "  '0'"
  ].join('\n');
  assert.strictEqual(out, expect);

  o.a = () => {};
  o.b = new Number(3);
  out = util.inspect(o, { compact: false, breakLength: 3 });
  expect = [
    '{',
    '  a: [Function],',
    '  b: [Number: 3]',
    '}'
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(o, { compact: false, breakLength: 3, showHidden: true });
  expect = [
    '{',
    '  a: [Function] {',
    '    [length]: 0,',
    "    [name]: ''",
    '  },',
    '  b: [Number: 3]',
    '}'
  ].join('\n');
  assert.strictEqual(out, expect);

  o[util.inspect.custom] = () => 42;
  out = util.inspect(o, { compact: false, breakLength: 3 });
  expect = '42';
  assert.strictEqual(out, expect);

  o[util.inspect.custom] = () => '12 45 78 01 34 67 90 23';
  out = util.inspect(o, { compact: false, breakLength: 3 });
  expect = '12 45 78 01 34 67 90 23';
  assert.strictEqual(out, expect);

  o[util.inspect.custom] = () => ({ a: '12 45 78 01 34 67 90 23' });
  out = util.inspect(o, { compact: false, breakLength: 3 });
  expect = "{\n  a: '12 45 78 01 34 ' +\n    '67 90 23'\n}";
  assert.strictEqual(out, expect);
}

// Check compact indentation.
{
  const typed = new Uint8Array();
  typed.buffer.foo = true;
  const set = new Set([[1, 2]]);
  const promise = Promise.resolve([[1, set]]);
  const map = new Map([[promise, typed]]);
  map.set(set.values(), map.values());

  let out = util.inspect(map, { compact: false, showHidden: true, depth: 9 });
  let expected = [
    'Map {',
    '  Promise {',
    '    [',
    '      [',
    '        1,',
    '        Set {',
    '          [',
    '            1,',
    '            2,',
    '            [length]: 2',
    '          ],',
    '          [size]: 1',
    '        },',
    '        [length]: 2',
    '      ],',
    '      [length]: 1',
    '    ]',
    '  } => Uint8Array [',
    '    [BYTES_PER_ELEMENT]: 1,',
    '    [length]: 0,',
    '    [byteLength]: 0,',
    '    [byteOffset]: 0,',
    '    [buffer]: ArrayBuffer {',
    '      byteLength: 0,',
    '      foo: true',
    '    }',
    '  ],',
    '  [Set Iterator] {',
    '    [',
    '      1,',
    '      2,',
    '      [length]: 2',
    '    ]',
    '  } => [Map Iterator] {',
    '    Uint8Array [',
    '      [BYTES_PER_ELEMENT]: 1,',
    '      [length]: 0,',
    '      [byteLength]: 0,',
    '      [byteOffset]: 0,',
    '      [buffer]: ArrayBuffer {',
    '        byteLength: 0,',
    '        foo: true',
    '      }',
    '    ],',
    '    [Circular]',
    '  },',
    '  [size]: 2',
    '}'
  ].join('\n');

  assert.strict.equal(out, expected);

  out = util.inspect(map, { showHidden: true, depth: 9, breakLength: 4 });
  expected = [
    'Map {',
    '  Promise {',
    '    [ [ 1,',
    '        Set {',
    '          [ 1,',
    '            2,',
    '            [length]: 2 ],',
    '          [size]: 1 },',
    '        [length]: 2 ],',
    '      [length]: 1 ] } => Uint8Array [',
    '    [BYTES_PER_ELEMENT]: 1,',
    '    [length]: 0,',
    '    [byteLength]: 0,',
    '    [byteOffset]: 0,',
    '    [buffer]: ArrayBuffer {',
    '      byteLength: 0,',
    '      foo: true } ],',
    '  [Set Iterator] {',
    '    [ 1,',
    '      2,',
    '      [length]: 2 ] } => [Map Iterator] {',
    '    Uint8Array [',
    '      [BYTES_PER_ELEMENT]: 1,',
    '      [length]: 0,',
    '      [byteLength]: 0,',
    '      [byteOffset]: 0,',
    '      [buffer]: ArrayBuffer {',
    '        byteLength: 0,',
    '        foo: true } ],',
    '    [Circular] },',
    '  [size]: 2 }'
  ].join('\n');

  assert.strict.equal(out, expected);
}

{ // Test WeakMap
  const obj = {};
  const arr = [];
  const weakMap = new WeakMap([[obj, arr], [arr, obj]]);
  let out = util.inspect(weakMap, { showHidden: true });
  let expect = 'WeakMap { [ [length]: 0 ] => {}, {} => [ [length]: 0 ] }';
  assert.strictEqual(out, expect);

  out = util.inspect(weakMap);
  expect = 'WeakMap { <items unknown> }';
  assert.strictEqual(out, expect);

  out = util.inspect(weakMap, { maxArrayLength: 0, showHidden: true });
  expect = 'WeakMap { ... 2 more items }';
  assert.strictEqual(out, expect);

  weakMap.extra = true;
  out = util.inspect(weakMap, { maxArrayLength: 1, showHidden: true });
  // It is not possible to determine the output reliable.
  expect = 'WeakMap { [ [length]: 0 ] => {}, ... 1 more item, extra: true }';
  const expectAlt = 'WeakMap { {} => [ [length]: 0 ], ... 1 more item, ' +
                    'extra: true }';
  assert(out === expect || out === expectAlt,
         `Found: "${out}"\nrather than: "${expect}"\nor: "${expectAlt}"`);
}

{ // Test WeakSet
  const weakSet = new WeakSet([{}, [1]]);
  let out = util.inspect(weakSet, { showHidden: true });
  let expect = 'WeakSet { [ 1, [length]: 1 ], {} }';
  assert.strictEqual(out, expect);

  out = util.inspect(weakSet);
  expect = 'WeakSet { <items unknown> }';
  assert.strictEqual(out, expect);

  out = util.inspect(weakSet, { maxArrayLength: -2, showHidden: true });
  expect = 'WeakSet { ... 2 more items }';
  assert.strictEqual(out, expect);

  weakSet.extra = true;
  out = util.inspect(weakSet, { maxArrayLength: 1, showHidden: true });
  // It is not possible to determine the output reliable.
  expect = 'WeakSet { {}, ... 1 more item, extra: true }';
  const expectAlt = 'WeakSet { [ 1, [length]: 1 ], ... 1 more item, ' +
                    'extra: true }';
  assert(out === expect || out === expectAlt,
         `Found: "${out}"\nrather than: "${expect}"\nor: "${expectAlt}"`);
}

{ // Test argument objects.
  const args = (function() { return arguments; })('a');
  assert.strictEqual(util.inspect(args), "[Arguments] { '0': 'a' }");
}

{
  // Test that a long linked list can be inspected without throwing an error.
  const list = {};
  let head = list;
  // A linked list of length 100k should be inspectable in some way, even though
  // the real cutoff value is much lower than 100k.
  for (let i = 0; i < 100000; i++)
    head = head.next = {};
  assert.strictEqual(
    util.inspect(list),
    '{ next: { next: { next: [Object] } } }'
  );
  const longList = util.inspect(list, { depth: Infinity });
  const match = longList.match(/next/g);
  assert(match.length > 1000 && match.length < 10000);
  assert(longList.includes('[Object: Inspection interrupted ' +
    'prematurely. Maximum call stack size exceeded.]'));
}

// Do not escape single quotes if no double quote or backtick is present.
assert.strictEqual(util.inspect("'"), '"\'"');
assert.strictEqual(util.inspect('"\''), '`"\'`');
// eslint-disable-next-line no-template-curly-in-string
assert.strictEqual(util.inspect('"\'${a}'), "'\"\\'${a}'");

// Verify that throwing in valueOf and toString still produces nice results.
[
  [new String(55), "[String: '55']"],
  [new Boolean(true), '[Boolean: true]'],
  [new Number(55), '[Number: 55]'],
  [Object(BigInt(55)), '[BigInt: 55n]'],
  [Object(Symbol('foo')), '[Symbol: Symbol(foo)]'],
  [function() {}, '[Function]'],
  [() => {}, '[Function]'],
  [[1, 2], '[ 1, 2 ]'],
  [[, , 5, , , , ], '[ <2 empty items>, 5, <3 empty items> ]'],
  [{ a: 5 }, '{ a: 5 }'],
  [new Set([1, 2]), 'Set { 1, 2 }'],
  [new Map([[1, 2]]), 'Map { 1 => 2 }'],
  [new Set([1, 2]).entries(), '[Set Iterator] { 1, 2 }'],
  [new Map([[1, 2]]).keys(), '[Map Iterator] { 1 }'],
  [new Date(2000), '1970-01-01T00:00:02.000Z'],
  [new Uint8Array(2), 'Uint8Array [ 0, 0 ]'],
  [new Promise((resolve) => setTimeout(resolve, 10)), 'Promise { <pending> }'],
  [new WeakSet(), 'WeakSet { <items unknown> }'],
  [new WeakMap(), 'WeakMap { <items unknown> }'],
  [/foobar/g, '/foobar/g']
].forEach(([value, expected]) => {
  Object.defineProperty(value, 'valueOf', {
    get() {
      throw new Error('valueOf');
    }
  });
  Object.defineProperty(value, 'toString', {
    get() {
      throw new Error('toString');
    }
  });
  assert.strictEqual(util.inspect(value), expected);
  value.foo = 'bar';
  assert.notStrictEqual(util.inspect(value), expected);
  delete value.foo;
  value[Symbol('foo')] = 'yeah';
  assert.notStrictEqual(util.inspect(value), expected);
});

// Verify that having no prototype still produces nice results.
[
  [[1, 3, 4], '[Array: null prototype] [ 1, 3, 4 ]'],
  [new Set([1, 2]), '[Set: null prototype] { 1, 2 }'],
  [new Map([[1, 2]]), '[Map: null prototype] { 1 => 2 }'],
  [new Promise((resolve) => setTimeout(resolve, 10)),
   '[Promise: null prototype] { <pending> }'],
  [new WeakSet(), '[WeakSet: null prototype] { <items unknown> }'],
  [new WeakMap(), '[WeakMap: null prototype] { <items unknown> }'],
  [new Uint8Array(2), '[Uint8Array: null prototype] [ 0, 0 ]'],
  [new Uint16Array(2), '[Uint16Array: null prototype] [ 0, 0 ]'],
  [new Uint32Array(2), '[Uint32Array: null prototype] [ 0, 0 ]'],
  [new Int8Array(2), '[Int8Array: null prototype] [ 0, 0 ]'],
  [new Int16Array(2), '[Int16Array: null prototype] [ 0, 0 ]'],
  [new Int32Array(2), '[Int32Array: null prototype] [ 0, 0 ]'],
  [new Float32Array(2), '[Float32Array: null prototype] [ 0, 0 ]'],
  [new Float64Array(2), '[Float64Array: null prototype] [ 0, 0 ]'],
  [new BigInt64Array(2), '[BigInt64Array: null prototype] [ 0n, 0n ]'],
  [new BigUint64Array(2), '[BigUint64Array: null prototype] [ 0n, 0n ]'],
  [new ArrayBuffer(16), '[ArrayBuffer: null prototype] {\n' +
     '  [Uint8Contents]: <00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00>,\n' +
     '  byteLength: undefined }'],
  [new DataView(new ArrayBuffer(16)),
   '[DataView: null prototype] {\n  byteLength: undefined,\n  ' +
     'byteOffset: undefined,\n  buffer: undefined }'],
  [new SharedArrayBuffer(2), '[SharedArrayBuffer: null prototype] ' +
     '{ [Uint8Contents]: <00 00>, byteLength: undefined }'],
  [/foobar/, '[RegExp: null prototype] /foobar/'],
  [new Date('Sun, 14 Feb 2010 11:48:40 GMT'),
   '[Date: null prototype] 2010-02-14T11:48:40.000Z']
].forEach(([value, expected]) => {
  assert.strictEqual(
    util.inspect(Object.setPrototypeOf(value, null)),
    expected
  );
  value.foo = 'bar';
  assert.notStrictEqual(util.inspect(value), expected);
  delete value.foo;
  value[Symbol('foo')] = 'yeah';
  assert.notStrictEqual(util.inspect(value), expected);
});

// Verify that subclasses with and without prototype produce nice results.
[
  [RegExp, ['foobar', 'g'], '/foobar/g'],
  [WeakSet, [[{}]], '{ <items unknown> }'],
  [WeakMap, [[[{}, {}]]], '{ <items unknown> }'],
  [BigInt64Array, [10], '[ 0n, 0n, 0n, 0n, 0n, 0n, 0n, 0n, 0n, 0n ]']
].forEach(([base, input, rawExpected]) => {
  class Foo extends base {}
  const value = new Foo(...input);
  const symbol = value[Symbol.toStringTag];
  const expected = `Foo ${symbol ? `[${symbol}] ` : ''}${rawExpected}`;
  const expectedWithoutProto = `[${base.name}: null prototype] ${rawExpected}`;
  assert.strictEqual(util.inspect(value), expected);
  value.foo = 'bar';
  assert.notStrictEqual(util.inspect(value), expected);
  delete value.foo;
  assert.strictEqual(
    util.inspect(Object.setPrototypeOf(value, null)),
    expectedWithoutProto
  );
  value.foo = 'bar';
  let res = util.inspect(value);
  assert.notStrictEqual(res, expectedWithoutProto);
  assert(/foo: 'bar'/.test(res), res);
  delete value.foo;
  value[Symbol('foo')] = 'yeah';
  res = util.inspect(value);
  assert.notStrictEqual(res, expectedWithoutProto);
  assert(/\[Symbol\(foo\)]: 'yeah'/.test(res), res);
});

// Date null prototype checks
{
  class CustomDate extends Date {
  }

  const date = new CustomDate('Sun, 14 Feb 2010 11:48:40 GMT');
  assert.strictEqual(util.inspect(date), 'CustomDate 2010-02-14T11:48:40.000Z');

  // add properties
  date.foo = 'bar';
  assert.strictEqual(util.inspect(date),
                     '{ CustomDate 2010-02-14T11:48:40.000Z foo: \'bar\' }');

  // check for null prototype
  Object.setPrototypeOf(date, null);
  assert.strictEqual(util.inspect(date),
                     '{ [Date: null prototype] 2010-02-14T11:48:40.000Z' +
                     ' foo: \'bar\' }');

  const anotherDate = new CustomDate('Sun, 14 Feb 2010 11:48:40 GMT');
  Object.setPrototypeOf(anotherDate, null);
  assert.strictEqual(util.inspect(anotherDate),
                     '[Date: null prototype] 2010-02-14T11:48:40.000Z');
}

// Check for invalid dates and null prototype
{
  class CustomDate extends Date {
  }

  const date = new CustomDate('invalid_date');
  assert.strictEqual(util.inspect(date), 'CustomDate Invalid Date');

  // add properties
  date.foo = 'bar';
  assert.strictEqual(util.inspect(date),
                     '{ CustomDate Invalid Date foo: \'bar\' }');

  // check for null prototype
  Object.setPrototypeOf(date, null);
  assert.strictEqual(util.inspect(date),
                     '{ [Date: null prototype] Invalid Date foo: \'bar\' }');
}

assert.strictEqual(inspect(1n), '1n');
assert.strictEqual(inspect(Object(-1n)), '[BigInt: -1n]');
assert.strictEqual(inspect(Object(13n)), '[BigInt: 13n]');
assert.strictEqual(inspect(new BigInt64Array([0n])), 'BigInt64Array [ 0n ]');
assert.strictEqual(inspect(new BigUint64Array([0n])), 'BigUint64Array [ 0n ]');

// Verify non-enumerable keys get escaped.
{
  const obj = {};
  Object.defineProperty(obj, 'Non\nenumerable\tkey', { value: true });
  assert.strictEqual(
    util.inspect(obj, { showHidden: true }),
    '{ [Non\\nenumerable\\tkey]: true }'
  );
}

// Check for special colors.
{
  const special = inspect.colors[inspect.styles.special];
  const string = inspect.colors[inspect.styles.string];

  assert.strictEqual(
    inspect(new WeakSet(), { colors: true }),
    `WeakSet { \u001b[${special[0]}m<items unknown>\u001b[${special[1]}m }`
  );
  assert.strictEqual(
    inspect(new WeakMap(), { colors: true }),
    `WeakMap { \u001b[${special[0]}m<items unknown>\u001b[${special[1]}m }`
  );
  assert.strictEqual(
    inspect(new Promise(() => {}), { colors: true }),
    `Promise { \u001b[${special[0]}m<pending>\u001b[${special[1]}m }`
  );

  const rejection = Promise.reject('Oh no!');
  assert.strictEqual(
    inspect(rejection, { colors: true }),
    `Promise { \u001b[${special[0]}m<rejected>\u001b[${special[1]}m ` +
    `\u001b[${string[0]}m'Oh no!'\u001b[${string[1]}m }`
  );
  rejection.catch(() => {});
}

assert.strictEqual(
  inspect([1, 3, 2], { sorted: true }),
  inspect([1, 3, 2])
);
assert.strictEqual(
  inspect({ c: 3, a: 1, b: 2 }, { sorted: true }),
  '{ a: 1, b: 2, c: 3 }'
);
assert.strictEqual(
  inspect(
    { a200: 4, a100: 1, a102: 3, a101: 2 },
    { sorted(a, b) { return b.localeCompare(a); } }
  ),
  '{ a200: 4, a102: 3, a101: 2, a100: 1 }'
);

// Non-indices array properties are sorted as well.
{
  const arr = [3, 2, 1];
  arr.b = 2;
  arr.c = 3;
  arr.a = 1;
  arr[Symbol('b')] = true;
  arr[Symbol('a')] = false;
  assert.strictEqual(
    inspect(arr, { sorted: true }),
    '[ 3, 2, 1, [Symbol(a)]: false, [Symbol(b)]: true, a: 1, b: 2, c: 3 ]'
  );
}

// Manipulate the prototype in weird ways.
{
  let obj = { a: true };
  let value = (function() { return function() {}; })();
  Object.setPrototypeOf(value, null);
  Object.setPrototypeOf(obj, value);
  assert.strictEqual(util.inspect(obj), '<[Function]> { a: true }');
  assert.strictEqual(
    util.inspect(obj, { colors: true }),
    '<\u001b[36m[Function]\u001b[39m> { a: \u001b[33mtrue\u001b[39m }'
  );

  obj = { a: true };
  value = [];
  Object.setPrototypeOf(value, null);
  Object.setPrototypeOf(obj, value);
  assert.strictEqual(
    util.inspect(obj),
    '<[Array: null prototype] []> { a: true }'
  );

  function StorageObject() {}
  StorageObject.prototype = Object.create(null);
  assert.strictEqual(
    util.inspect(new StorageObject()),
    '<[Object: null prototype] {}> {}'
  );

  obj = [1, 2, 3];
  Object.setPrototypeOf(obj, Number.prototype);
  assert.strictEqual(inspect(obj), "Number { '0': 1, '1': 2, '2': 3 }");

  Object.setPrototypeOf(obj, Object.create(null));
  assert.strictEqual(
    inspect(obj),
    "<[Object: null prototype] {}> { '0': 1, '1': 2, '2': 3 }"
  );
}

// Check that the fallback always works.
{
  const obj = new Set([1, 2]);
  const iterator = obj[Symbol.iterator];
  Object.setPrototypeOf(obj, null);
  Object.defineProperty(obj, Symbol.iterator, {
    value: iterator,
    configurable: true
  });
  assert.strictEqual(util.inspect(obj), '[Set: null prototype] { 1, 2 }');
}

// Check the getter option.
{
  let foo = 1;
  const get = { get foo() { return foo; } };
  const getset = {
    get foo() { return foo; },
    set foo(val) { foo = val; },
    get inc() { return ++foo; }
  };
  const thrower = { get foo() { throw new Error('Oops'); } };
  assert.strictEqual(
    inspect(get, { getters: true, colors: true }),
    '{ foo: \u001b[36m[Getter:\u001b[39m ' +
      '\u001b[33m1\u001b[39m\u001b[36m]\u001b[39m }');
  assert.strictEqual(
    inspect(thrower, { getters: true }),
    '{ foo: [Getter: <Inspection threw (Oops)>] }');
  assert.strictEqual(
    inspect(getset, { getters: true }),
    '{ foo: [Getter/Setter: 1], inc: [Getter: 2] }');
  assert.strictEqual(
    inspect(getset, { getters: 'get' }),
    '{ foo: [Getter/Setter], inc: [Getter: 3] }');
  assert.strictEqual(
    inspect(getset, { getters: 'set' }),
    '{ foo: [Getter/Setter: 3], inc: [Getter] }');
  getset.foo = new Set([[{ a: true }, 2, {}], 'foobar', { x: 1 }]);
  assert.strictEqual(
    inspect(getset, { getters: true }),
    '{ foo: [Getter/Setter] Set { [ [Object], 2, {} ], ' +
      "'foobar', { x: 1 } },\n  inc: [Getter: NaN] }");
}
