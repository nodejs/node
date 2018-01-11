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

// Flags: --expose_internals
'use strict';
const common = require('../common');
const assert = require('assert');
const JSStream = process.binding('js_stream').JSStream;
const util = require('util');
const vm = require('vm');
const { previewMapIterator } = require('internal/v8');

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
  `'${Array(75).fill(1)}\\'\\n\\u001d\\n\\u0003'`
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
                   '{ a: { b: { c: { d: 2 } } } }');
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
assert.strictEqual(util.inspect(new Uint8Array(0)), 'Uint8Array [  ]');
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
  '{ [String: \'hello\'] [length]: 5, [Symbol(foo)]: 123 }'
);

assert.strictEqual(util.inspect((new JSStream())._externalStream),
                   '[External]');

{
  const regexp = /regexp/;
  regexp.aprop = 42;
  assert.strictEqual(util.inspect({ a: regexp }, false, 0), '{ a: /regexp/ }');
}

assert(!/Object/.test(
  util.inspect({ a: { a: { a: { a: {} } } } }, undefined, undefined, true)
));
assert(!/Object/.test(
  util.inspect({ a: { a: { a: { a: {} } } } }, undefined, null, true)
));

for (const showHidden of [true, false]) {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab, 1, 2);
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(new DataView(ab, 1, 2), showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4 } }');
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.strictEqual(util.inspect(ab, showHidden),
                     'ArrayBuffer { byteLength: 4, x: 42 }');
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4, x: 42 },\n' +
                     '  y: 1337 }');
}

// Now do the same checks but from a different context
for (const showHidden of [true, false]) {
  const ab = vm.runInNewContext('new ArrayBuffer(4)');
  const dv = vm.runInNewContext('new DataView(ab, 1, 2)', { ab });
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(new DataView(ab, 1, 2), showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4 } }');
  assert.strictEqual(
    util.inspect(ab, showHidden),
    'ArrayBuffer { byteLength: 4 }'
  );
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4 } }');
  ab.x = 42;
  dv.y = 1337;
  assert.strictEqual(util.inspect(ab, showHidden),
                     'ArrayBuffer { byteLength: 4, x: 42 }');
  assert.strictEqual(util.inspect(dv, showHidden),
                     'DataView {\n' +
                     '  byteLength: 2,\n' +
                     '  byteOffset: 1,\n' +
                     '  buffer: ArrayBuffer { byteLength: 4, x: 42 },\n' +
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
// Objects without prototype
assert.strictEqual(
  util.inspect(Object.create(null, {
    name: { value: 'Tim', enumerable: true },
    hidden: { value: 'secret' }
  }), { showHidden: true }),
  "{ name: 'Tim', [hidden]: 'secret' }"
);

assert.strictEqual(
  util.inspect(Object.create(null, {
    name: { value: 'Tim', enumerable: true },
    hidden: { value: 'secret' }
  })),
  '{ name: \'Tim\' }'
);

// Dynamic properties
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
  value['a'] = value;
  assert.strictEqual(util.inspect(value), '{ a: [Circular] }');
}

// Array with dynamic properties
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
                     '[ 1, 2, 3, growingLength: [Getter], \'-1\': -1 ]');
}

// Array with inherited number properties
{
  class CustomArray extends Array {}
  CustomArray.prototype[5] = 'foo';
  const arr = new CustomArray(50);
  assert.strictEqual(util.inspect(arr), 'CustomArray [ <50 empty items> ]');
}

// Array with extra properties
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

  const arr3 = [];
  arr3[-1] = -1;
  assert.strictEqual(util.inspect(arr3), "[ '-1': -1 ]");
}

// Indices out of bounds
{
  const arr = [];
  arr[2 ** 32] = true; // not a valid array index
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

// Function with properties
{
  const value = () => {};
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ [Function: value] aprop: 42 }');
}

// Anonymous function with properties
{
  const value = (() => function() {})();
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ [Function] aprop: 42 }');
}

// Regular expressions with properties
{
  const value = /123/ig;
  value.aprop = 42;
  assert.strictEqual(util.inspect(value), '{ /123/gi aprop: 42 }');
}

// Dates with properties
{
  const value = new Date('Sun, 14 Feb 2010 11:48:40 GMT');
  value.aprop = 42;
  assert.strictEqual(util.inspect(value),
                     '{ 2010-02-14T11:48:40.000Z aprop: 42 }');
}

// test the internal isDate implementation
{
  const Date2 = vm.runInNewContext('Date');
  const d = new Date2();
  const orig = util.inspect(d);
  Date2.prototype.foo = 'bar';
  const after = util.inspect(d);
  assert.strictEqual(orig, after);
}

// test positive/negative zero
assert.strictEqual(util.inspect(0), '0');
assert.strictEqual(util.inspect(-0), '-0');
// edge case from check
assert.strictEqual(util.inspect(-5e-324), '-5e-324');

// test for sparse array
{
  const a = ['foo', 'bar', 'baz'];
  assert.strictEqual(util.inspect(a), '[ \'foo\', \'bar\', \'baz\' ]');
  delete a[1];
  assert.strictEqual(util.inspect(a), '[ \'foo\', <1 empty item>, \'baz\' ]');
  assert.strictEqual(
    util.inspect(a, true),
    '[ \'foo\', <1 empty item>, \'baz\', [length]: 3 ]'
  );
  assert.strictEqual(util.inspect(new Array(5)), '[ <5 empty items> ]');
  a[3] = 'bar';
  a[100] = 'qux';
  assert.strictEqual(
    util.inspect(a, { breakLength: Infinity }),
    '[ \'foo\', <1 empty item>, \'baz\', \'bar\', <96 empty items>, \'qux\' ]'
  );
  delete a[3];
  assert.strictEqual(
    util.inspect(a, { maxArrayLength: 4 }),
    '[ \'foo\', <1 empty item>, \'baz\', <97 empty items>, ... 1 more item ]'
  );
}

// test for Array constructor in different context
{
  const map = new Map();
  map.set(1, 2);
  const vals = previewMapIterator(map.entries(), 100);
  const valsOutput = [];
  for (const o of vals) {
    valsOutput.push(o);
  }

  assert.strictEqual(util.inspect(valsOutput), '[ [ 1, 2 ] ]');
}

// test for other constructors in different context
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

// test for property descriptors
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
  assert.strictEqual(util.inspect(getter, true), '{ [a]: [Getter] }');
  assert.strictEqual(util.inspect(setter, true), '{ [b]: [Setter] }');
  assert.strictEqual(
    util.inspect(getterAndSetter, true),
    '{ [c]: [Getter/Setter] }'
  );
}

// exceptions should print the error message, not '{}'
{
  const errors = [];
  errors.push(new Error());
  errors.push(new Error('FAIL'));
  errors.push(new TypeError('FAIL'));
  errors.push(new SyntaxError('FAIL'));
  errors.forEach((err) => {
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

// Doesn't capture stack trace
{
  function BadCustomError(msg) {
    Error.call(this);
    Object.defineProperty(this, 'message',
                          { value: msg, enumerable: false });
    Object.defineProperty(this, 'name',
                          { value: 'BadCustomError', enumerable: false });
  }
  util.inherits(BadCustomError, Error);
  assert.strictEqual(
    util.inspect(new BadCustomError('foo')),
    '[BadCustomError: foo]'
  );
}

// GH-1941
// should not throw:
assert.strictEqual(util.inspect(Object.create(Date.prototype)), 'Date {}');

// GH-1944
assert.doesNotThrow(() => {
  const d = new Date();
  d.toUTCString = null;
  util.inspect(d);
});

assert.doesNotThrow(() => {
  const d = new Date();
  d.toISOString = null;
  util.inspect(d);
});

assert.doesNotThrow(() => {
  const r = /regexp/;
  r.toString = null;
  util.inspect(r);
});

// bug with user-supplied inspect function returns non-string
assert.doesNotThrow(() => {
  util.inspect([{
    inspect: () => 123
  }]);
});

// GH-2225
{
  const x = { inspect: util.inspect };
  assert.strictEqual(util.inspect(x).includes('inspect'), true);
}

// util.inspect should display the escaped value of a key.
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
    '{ \'\\\\\': 1, \'\\\\\\\\\': 2, \'\\\\\\\\\\\\\': 3, ' +
    '\'\\\\\\\\\\\\\\\\\': 4, \'\\n\': 5, \'\\r\': 6 }'
  );
  assert.strictEqual(
    util.inspect(y),
    '[ \'a\', \'b\', \'c\', \'\\\\\\\\\': \'d\', ' +
    '\'\\n\': \'e\', \'\\r\': \'f\' ]'
  );
}

// util.inspect.styles and util.inspect.colors
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

// an object with "hasOwnProperty" overwritten should not throw
assert.doesNotThrow(() => {
  util.inspect({
    hasOwnProperty: null
  });
});

// new API, accepts an "options" object
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
  // "customInspect" option can enable/disable calling inspect() on objects
  const subject = { inspect: () => 123 };

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

  // custom inspect() functions should be able to return other Objects
  subject.inspect = () => ({ foo: 'bar' });

  assert.strictEqual(util.inspect(subject), '{ foo: \'bar\' }');

  subject.inspect = (depth, opts) => {
    assert.strictEqual(opts.customInspectOptions, true);
  };

  util.inspect(subject, { customInspectOptions: true });
}

{
  // "customInspect" option can enable/disable calling [util.inspect.custom]()
  const subject = { [util.inspect.custom]: () => 123 };

  assert.strictEqual(
    util.inspect(subject, { customInspect: true }).includes('123'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: false }).includes('123'),
    false
  );

  // a custom [util.inspect.custom]() should be able to return other Objects
  subject[util.inspect.custom] = () => ({ foo: 'bar' });

  assert.strictEqual(util.inspect(subject), '{ foo: \'bar\' }');

  subject[util.inspect.custom] = (depth, opts) => {
    assert.strictEqual(opts.customInspectOptions, true);
  };

  util.inspect(subject, { customInspectOptions: true });
}

{
  // [util.inspect.custom] takes precedence over inspect
  const subject = {
    [util.inspect.custom]() { return 123; },
    inspect() { return 456; }
  };

  assert.strictEqual(
    util.inspect(subject, { customInspect: true }).includes('123'),
    true
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: false }).includes('123'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: true }).includes('456'),
    false
  );
  assert.strictEqual(
    util.inspect(subject, { customInspect: false }).includes('456'),
    false
  );
}

{
  // Returning `this` from a custom inspection function works.
  assert.strictEqual(util.inspect({ a: 123, inspect() { return this; } }),
                     '{ a: 123, inspect: [Function: inspect] }');

  const subject = { a: 123, [util.inspect.custom]() { return this; } };
  const UIC = 'util.inspect.custom';
  assert.strictEqual(util.inspect(subject),
                     `{ a: 123,\n  [Symbol(${UIC})]: [Function: [${UIC}]] }`);
}

// util.inspect with "colors" option should produce as many lines as without it
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

// test boxed primitives output the correct values
assert.strictEqual(util.inspect(new String('test')), '[String: \'test\']');
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

// test boxed primitives with own properties
{
  const str = new String('baz');
  str.foo = 'bar';
  assert.strictEqual(util.inspect(str), '{ [String: \'baz\'] foo: \'bar\' }');

  const bool = new Boolean(true);
  bool.foo = 'bar';
  assert.strictEqual(util.inspect(bool), '{ [Boolean: true] foo: \'bar\' }');

  const num = new Number(13.37);
  num.foo = 'bar';
  assert.strictEqual(util.inspect(num), '{ [Number: 13.37] foo: \'bar\' }');
}

// test es6 Symbol
if (typeof Symbol !== 'undefined') {
  assert.strictEqual(util.inspect(Symbol()), 'Symbol()');
  assert.strictEqual(util.inspect(Symbol(123)), 'Symbol(123)');
  assert.strictEqual(util.inspect(Symbol('hi')), 'Symbol(hi)');
  assert.strictEqual(util.inspect([Symbol()]), '[ Symbol() ]');
  assert.strictEqual(util.inspect({ foo: Symbol() }), '{ foo: Symbol() }');

  const options = { showHidden: true };
  let subject = {};

  subject[Symbol('symbol')] = 42;

  assert.strictEqual(util.inspect(subject), '{ [Symbol(symbol)]: 42 }');
  assert.strictEqual(
    util.inspect(subject, options),
    '{ [Symbol(symbol)]: 42 }'
  );

  Object.defineProperty(
    subject,
    Symbol(),
    { enumerable: false, value: 'non-enum' });
  assert.strictEqual(util.inspect(subject), '{ [Symbol(symbol)]: 42 }');
  assert.strictEqual(
    util.inspect(subject, options),
    '{ [Symbol(symbol)]: 42, [Symbol()]: \'non-enum\' }'
  );

  subject = [1, 2, 3];
  subject[Symbol('symbol')] = 42;

  assert.strictEqual(util.inspect(subject),
                     '[ 1, 2, 3, [Symbol(symbol)]: 42 ]');
}

// test Set
{
  assert.strictEqual(util.inspect(new Set()), 'Set {}');
  assert.strictEqual(util.inspect(new Set([1, 2, 3])), 'Set { 1, 2, 3 }');
  const set = new Set(['foo']);
  set.bar = 42;
  assert.strictEqual(
    util.inspect(set, true),
    'Set { \'foo\', [size]: 1, bar: 42 }'
  );
}

// Test circular Set
{
  const set = new Set();
  set.add(set);
  assert.strictEqual(util.inspect(set), 'Set { [Circular] }');
}

// test Map
{
  assert.strictEqual(util.inspect(new Map()), 'Map {}');
  assert.strictEqual(util.inspect(new Map([[1, 'a'], [2, 'b'], [3, 'c']])),
                     'Map { 1 => \'a\', 2 => \'b\', 3 => \'c\' }');
  const map = new Map([['foo', null]]);
  map.bar = 42;
  assert.strictEqual(util.inspect(map, true),
                     'Map { \'foo\' => null, [size]: 1, bar: 42 }');
}

// Test circular Map
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

// test Promise
{
  const resolved = Promise.resolve(3);
  assert.strictEqual(util.inspect(resolved), 'Promise { 3 }');

  const rejected = Promise.reject(3);
  assert.strictEqual(util.inspect(rejected), 'Promise { <rejected> 3 }');
  // squelch UnhandledPromiseRejection
  rejected.catch(() => {});

  const pending = new Promise(() => {});
  assert.strictEqual(util.inspect(pending), 'Promise { <pending> }');

  const promiseWithProperty = Promise.resolve('foo');
  promiseWithProperty.bar = 42;
  assert.strictEqual(util.inspect(promiseWithProperty),
                     'Promise { \'foo\', bar: 42 }');
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

// Test Map iterators
{
  const map = new Map([['foo', 'bar']]);
  assert.strictEqual(util.inspect(map.keys()), '[Map Iterator] { \'foo\' }');
  assert.strictEqual(util.inspect(map.values()), '[Map Iterator] { \'bar\' }');
  assert.strictEqual(util.inspect(map.entries()),
                     '[Map Iterator] { [ \'foo\', \'bar\' ] }');
  // make sure the iterator doesn't get consumed
  const keys = map.keys();
  assert.strictEqual(util.inspect(keys), '[Map Iterator] { \'foo\' }');
  assert.strictEqual(util.inspect(keys), '[Map Iterator] { \'foo\' }');
}

// Test Set iterators
{
  const aSet = new Set([1, 3]);
  assert.strictEqual(util.inspect(aSet.keys()), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(aSet.values()), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(aSet.entries()),
                     '[Set Iterator] { [ 1, 1 ], [ 3, 3 ] }');
  // make sure the iterator doesn't get consumed
  const keys = aSet.keys();
  assert.strictEqual(util.inspect(keys), '[Set Iterator] { 1, 3 }');
  assert.strictEqual(util.inspect(keys), '[Set Iterator] { 1, 3 }');
}

// Test alignment of items in container
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
          assert.strictEqual(pos, npos, 'container items not aligned');
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


// Test display of constructors
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
                     'MapSubclass [Map] { \'foo\' => 42 }');
  assert.strictEqual(util.inspect(new PromiseSubclass(() => {})),
                     'PromiseSubclass [Promise] { <pending> }');
  assert.strictEqual(
    util.inspect({ a: { b: new ArraySubclass([1, [2], 3]) } }, { depth: 1 }),
    '{ a: { b: [ArraySubclass] } }'
  );
}

// Empty and circular before depth
{
  const arr = [[[[]]]];
  assert.strictEqual(util.inspect(arr, { depth: 2 }), '[ [ [ [] ] ] ]');
  arr[0][0][0][0] = [];
  assert.strictEqual(util.inspect(arr, { depth: 2 }), '[ [ [ [Array] ] ] ]');
  arr[0][0][0] = {};
  assert.strictEqual(util.inspect(arr, { depth: 2 }), '[ [ [ {} ] ] ]');
  arr[0][0][0] = { a: 2 };
  assert.strictEqual(util.inspect(arr, { depth: 2 }), '[ [ [ [Object] ] ] ]');
  arr[0][0][0] = arr;
  assert.strictEqual(util.inspect(arr, { depth: 2 }), '[ [ [ [Circular] ] ] ]');
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
  assert.strictEqual(util.inspect(x), '{}');
}

{
  const x = [];
  x[''] = 1;
  assert.strictEqual(util.inspect(x), '[ \'\': 1 ]');
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

  assert.strictEqual(oneLine, '{ foo: \'abc\', bar: \'xyz\' }');
  assert.strictEqual(oneLine,
                     util.inspect(obj, { breakLength: breakpoint + 1 }));
  assert.strictEqual(twoLines, '{ foo: \'abc\',\n  bar: \'xyz\' }');
}

// util.inspect.defaultOptions tests
{
  const arr = new Array(101).fill();
  const obj = { a: { a: { a: { a: 1 } } } };

  const oldOptions = Object.assign({}, util.inspect.defaultOptions);

  // Set single option through property assignment
  util.inspect.defaultOptions.maxArrayLength = null;
  assert(!/1 more item/.test(util.inspect(arr)));
  util.inspect.defaultOptions.maxArrayLength = oldOptions.maxArrayLength;
  assert(/1 more item/.test(util.inspect(arr)));
  util.inspect.defaultOptions.depth = 2;
  assert(/Object/.test(util.inspect(obj)));
  util.inspect.defaultOptions.depth = oldOptions.depth;
  assert(!/Object/.test(util.inspect(obj)));
  assert.strictEqual(
    JSON.stringify(util.inspect.defaultOptions),
    JSON.stringify(oldOptions)
  );

  // Set multiple options through object assignment
  util.inspect.defaultOptions = { maxArrayLength: null, depth: 2 };
  assert(!/1 more item/.test(util.inspect(arr)));
  assert(/Object/.test(util.inspect(obj)));
  util.inspect.defaultOptions = oldOptions;
  assert(/1 more item/.test(util.inspect(arr)));
  assert(!/Object/.test(util.inspect(obj)));
  assert.strictEqual(
    JSON.stringify(util.inspect.defaultOptions),
    JSON.stringify(oldOptions)
  );

  common.expectsError(() => {
    util.inspect.defaultOptions = null;
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type Object'
  }
  );

  common.expectsError(() => {
    util.inspect.defaultOptions = 'bad';
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type Object'
  }
  );
}

assert.doesNotThrow(() => util.inspect(process));

// Setting custom inspect property to a non-function should do nothing.
{
  const obj = { inspect: 'fhqwhgads' };
  assert.strictEqual(util.inspect(obj), "{ inspect: 'fhqwhgads' }");
}

{
  // @@toStringTag
  assert.strictEqual(util.inspect({ [Symbol.toStringTag]: 'a' }),
                     'Object [a] { [Symbol(Symbol.toStringTag)]: \'a\' }');

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
                     '[foo] {}');

  assert.strictEqual(util.inspect(new Foo()), 'Foo [bar] { foo: \'bar\' }');

  assert.strictEqual(
    util.inspect(new (class extends Foo {})()),
    'Foo [bar] { foo: \'bar\' }');

  assert.strictEqual(
    util.inspect(Object.create(Object.create(Foo.prototype), {
      foo: { value: 'bar', enumerable: true }
    })),
    'Foo [bar] { foo: \'bar\' }');

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
    '{ a: ',
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
    '        \'Lorem ipsum dolor\\nsit amet,\\tconsectetur \' +',
    '          \'adipiscing elit, sed do eiusmod tempor \' +',
    '          \'incididunt ut labore et dolore magna \' +',
    '          \'aliqua.\',',
    '        \'test\',',
    '        \'foo\'',
    '      ]',
    '    ],',
    '    4',
    '  ],',
    '  b: Map {',
    '    \'za\' => 1,',
    '    \'zb\' => \'test\'',
    '  }',
    '}'
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(o.a[2][0][0], { compact: false, breakLength: 30 });
  expect = [
    '\'Lorem ipsum dolor\\nsit \' +',
    '  \'amet,\\tconsectetur \' +',
    '  \'adipiscing elit, sed do \' +',
    '  \'eiusmod tempor incididunt \' +',
    '  \'ut labore et dolore magna \' +',
    '  \'aliqua.\''
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12345678901234567890123456789012345678901234567890',
    { compact: false, breakLength: 3 });
  expect = '\'12345678901234567890123456789012345678901234567890\'';
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 123456789012345678901234567890',
    { compact: false, breakLength: 3 });
  expect = [
    '\'12 45 78 01 34 \' +',
    '  \'67 90 23 56 89 \' +',
    '  \'123456789012345678901234567890\''
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 1234567890123 0',
    { compact: false, breakLength: 3 });
  expect = [
    '\'12 45 78 01 34 \' +',
    '  \'67 90 23 56 89 \' +',
    '  \'1234567890123 0\''
  ].join('\n');
  assert.strictEqual(out, expect);

  out = util.inspect(
    '12 45 78 01 34 67 90 23 56 89 12345678901234567 0',
    { compact: false, breakLength: 3 });
  expect = [
    '\'12 45 78 01 34 \' +',
    '  \'67 90 23 56 89 \' +',
    '  \'12345678901234567 \' +',
    '  \'0\''
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
  expect = '{\n  a: \'12 45 78 01 34 \' +\n    \'67 90 23\'\n}';
  assert.strictEqual(out, expect);
}
