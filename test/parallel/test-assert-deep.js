'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');

// Template tag function turning an error message into a RegExp
// for assert.throws()
function re(literals, ...values) {
  let result = literals[0];
  const escapeRE = /[\\^$.*+?()[\]{}|=!<>:-]/g;
  for (const [i, value] of values.entries()) {
    const str = util.inspect(value);
    // Need to escape special characters.
    result += str.replace(escapeRE, '\\$&');
    result += literals[i + 1];
  }
  return common.expectsError({
    code: 'ERR_ASSERTION',
    message: new RegExp(`^${result}$`)
  });
}

// The following deepEqual tests might seem very weird.
// They just describe what it is now.
// That is why we discourage using deepEqual in our own tests.

// Turn off no-restricted-properties because we are testing deepEqual!
/* eslint-disable no-restricted-properties */

const arr = new Uint8Array([120, 121, 122, 10]);
const buf = Buffer.from(arr);
// They have different [[Prototype]]
assert.throws(() => assert.deepStrictEqual(arr, buf),
              re`${arr} deepStrictEqual ${buf}`);
assert.doesNotThrow(() => assert.deepEqual(arr, buf));

{
  const buf2 = Buffer.from(arr);
  buf2.prop = 1;

  assert.throws(() => assert.deepStrictEqual(buf2, buf),
                re`${buf2} deepStrictEqual ${buf}`);
  assert.doesNotThrow(() => assert.deepEqual(buf2, buf));
}

{
  const arr2 = new Uint8Array([120, 121, 122, 10]);
  arr2.prop = 5;
  assert.throws(() => assert.deepStrictEqual(arr, arr2),
                re`${arr} deepStrictEqual ${arr2}`);
  assert.doesNotThrow(() => assert.deepEqual(arr, arr2));
}

const date = new Date('2016');

class MyDate extends Date {
  constructor(...args) {
    super(...args);
    this[0] = '1';
  }
}

const date2 = new MyDate('2016');

// deepEqual returns true as long as the time are the same,
// but deepStrictEqual checks own properties
assert.doesNotThrow(() => assert.deepEqual(date, date2));
assert.doesNotThrow(() => assert.deepEqual(date2, date));
assert.throws(() => assert.deepStrictEqual(date, date2),
              re`${date} deepStrictEqual ${date2}`);
assert.throws(() => assert.deepStrictEqual(date2, date),
              re`${date2} deepStrictEqual ${date}`);

class MyRegExp extends RegExp {
  constructor(...args) {
    super(...args);
    this[0] = '1';
  }
}

const re1 = new RegExp('test');
const re2 = new MyRegExp('test');

// deepEqual returns true as long as the regexp-specific properties
// are the same, but deepStrictEqual checks all properties
assert.doesNotThrow(() => assert.deepEqual(re1, re2));
assert.throws(() => assert.deepStrictEqual(re1, re2),
              re`${re1} deepStrictEqual ${re2}`);

// For these weird cases, deepEqual should pass (at least for now),
// but deepStrictEqual should throw.
{
  const similar = new Set([
    { 0: '1' },  // Object
    { 0: 1 },  // Object
    new String('1'),  // Object
    ['1'],  // Array
    [1],  // Array
    date2,  // Date with this[0] = '1'
    re2,  // RegExp with this[0] = '1'
    new Int8Array([1]), // Int8Array
    new Uint8Array([1]), // Uint8Array
    new Int16Array([1]), // Int16Array
    new Uint16Array([1]), // Uint16Array
    new Int32Array([1]), // Int32Array
    new Uint32Array([1]), // Uint32Array
    Buffer.from([1]),
    // Arguments {'0': '1'} is not here
    // See https://github.com/nodejs/node-v0.x-archive/pull/7178
  ]);

  for (const a of similar) {
    for (const b of similar) {
      if (a !== b) {
        assert.deepEqual(a, b);
        assert.throws(() => assert.deepStrictEqual(a, b),
                      re`${a} deepStrictEqual ${b}`);
      }
    }
  }
}

common.expectsError(() => {
  assert.deepEqual(new Set([{ a: 0 }]), new Set([{ a: 1 }]));
}, {
  code: 'ERR_ASSERTION',
  message: /^Set { { a: 0 } } deepEqual Set { { a: 1 } }$/
});

function assertDeepAndStrictEqual(a, b) {
  assert.deepEqual(a, b);
  assert.deepStrictEqual(a, b);

  assert.deepEqual(b, a);
  assert.deepStrictEqual(b, a);
}

function assertNotDeepOrStrict(a, b, err) {
  assert.throws(() => assert.deepEqual(a, b), err || re`${a} deepEqual ${b}`);
  assert.throws(() => assert.deepStrictEqual(a, b), err ||
                re`${a} deepStrictEqual ${b}`);

  assert.throws(() => assert.deepEqual(b, a), err || re`${b} deepEqual ${a}`);
  assert.throws(() => assert.deepStrictEqual(b, a), err ||
                re`${b} deepStrictEqual ${a}`);
}

function assertOnlyDeepEqual(a, b, err) {
  assert.doesNotThrow(() => assert.deepEqual(a, b));
  assert.throws(() => assert.deepStrictEqual(a, b), err ||
                re`${a} deepStrictEqual ${b}`);

  assert.doesNotThrow(() => assert.deepEqual(b, a));
  assert.throws(() => assert.deepStrictEqual(b, a), err ||
                re`${b} deepStrictEqual ${a}`);
}

// es6 Maps and Sets
assertDeepAndStrictEqual(new Set(), new Set());
assertDeepAndStrictEqual(new Map(), new Map());

assertDeepAndStrictEqual(new Set([1, 2, 3]), new Set([1, 2, 3]));
assertNotDeepOrStrict(new Set([1, 2, 3]), new Set([1, 2, 3, 4]));
assertNotDeepOrStrict(new Set([1, 2, 3, 4]), new Set([1, 2, 3]));
assertDeepAndStrictEqual(new Set(['1', '2', '3']), new Set(['1', '2', '3']));
assertDeepAndStrictEqual(new Set([[1, 2], [3, 4]]), new Set([[3, 4], [1, 2]]));

{
  const a = [ 1, 2 ];
  const b = [ 3, 4 ];
  const c = [ 1, 2 ];
  const d = [ 3, 4 ];

  assertDeepAndStrictEqual(
    { a: a, b: b, s: new Set([a, b]) },
    { a: c, b: d, s: new Set([d, c]) }
  );
}

assertDeepAndStrictEqual(new Map([[1, 1], [2, 2]]), new Map([[1, 1], [2, 2]]));
assertDeepAndStrictEqual(new Map([[1, 1], [2, 2]]), new Map([[2, 2], [1, 1]]));
assertNotDeepOrStrict(new Map([[1, 1], [2, 2]]), new Map([[1, 2], [2, 1]]));
assertNotDeepOrStrict(
  new Map([[[1], 1], [{}, 2]]),
  new Map([[[1], 2], [{}, 1]])
);

assertNotDeepOrStrict(new Set([1]), [1]);
assertNotDeepOrStrict(new Set(), []);
assertNotDeepOrStrict(new Set(), {});

assertNotDeepOrStrict(new Map([['a', 1]]), { a: 1 });
assertNotDeepOrStrict(new Map(), []);
assertNotDeepOrStrict(new Map(), {});

assertOnlyDeepEqual(new Set(['1']), new Set([1]));

assertOnlyDeepEqual(new Map([['1', 'a']]), new Map([[1, 'a']]));
assertOnlyDeepEqual(new Map([['a', '1']]), new Map([['a', 1]]));
assertNotDeepOrStrict(new Map([['a', '1']]), new Map([['a', 2]]));

assertDeepAndStrictEqual(new Set([{}]), new Set([{}]));

// Ref: https://github.com/nodejs/node/issues/13347
assertNotDeepOrStrict(
  new Set([{ a: 1 }, { a: 1 }]),
  new Set([{ a: 1 }, { a: 2 }])
);
assertNotDeepOrStrict(
  new Set([{ a: 1 }, { a: 1 }, { a: 2 }]),
  new Set([{ a: 1 }, { a: 2 }, { a: 2 }])
);
assertNotDeepOrStrict(
  new Map([[{ x: 1 }, 5], [{ x: 1 }, 5]]),
  new Map([[{ x: 1 }, 5], [{ x: 2 }, 5]])
);

assertNotDeepOrStrict(new Set([3, '3']), new Set([3, 4]));
assertNotDeepOrStrict(new Map([[3, 0], ['3', 0]]), new Map([[3, 0], [4, 0]]));

assertNotDeepOrStrict(
  new Set([{ a: 1 }, { a: 1 }, { a: 2 }]),
  new Set([{ a: 1 }, { a: 2 }, { a: 2 }])
);

// Mixed primitive and object keys
assertDeepAndStrictEqual(
  new Map([[1, 'a'], [{}, 'a']]),
  new Map([[1, 'a'], [{}, 'a']])
);
assertDeepAndStrictEqual(
  new Set([1, 'a', [{}, 'a']]),
  new Set([1, 'a', [{}, 'a']])
);

// This is an awful case, where a map contains multiple equivalent keys:
assertOnlyDeepEqual(
  new Map([[1, 'a'], ['1', 'b']]),
  new Map([['1', 'a'], [true, 'b']])
);
assertNotDeepOrStrict(
  new Set(['a']),
  new Set(['b'])
);
assertDeepAndStrictEqual(
  new Map([[{}, 'a'], [{}, 'b']]),
  new Map([[{}, 'b'], [{}, 'a']])
);
assertOnlyDeepEqual(
  new Map([[true, 'a'], ['1', 'b'], [1, 'a']]),
  new Map([['1', 'a'], [1, 'b'], [true, 'a']])
);
assertNotDeepOrStrict(
  new Map([[true, 'a'], ['1', 'b'], [1, 'c']]),
  new Map([['1', 'a'], [1, 'b'], [true, 'a']])
);

// Similar object keys
assertNotDeepOrStrict(
  new Set([{}, {}]),
  new Set([{}, 1])
);
assertNotDeepOrStrict(
  new Set([[{}, 1], [{}, 1]]),
  new Set([[{}, 1], [1, 1]])
);
assertNotDeepOrStrict(
  new Map([[{}, 1], [{}, 1]]),
  new Map([[{}, 1], [1, 1]])
);
assertOnlyDeepEqual(
  new Map([[{}, 1], [true, 1]]),
  new Map([[{}, 1], [1, 1]])
);

// Similar primitive key / values
assertNotDeepOrStrict(
  new Set([1, true, false]),
  new Set(['1', 0, '0'])
);
assertNotDeepOrStrict(
  new Map([[1, 5], [true, 5], [false, 5]]),
  new Map([['1', 5], [0, 5], ['0', 5]])
);

// undefined value in Map
assertDeepAndStrictEqual(
  new Map([[1, undefined]]),
  new Map([[1, undefined]])
);
assertOnlyDeepEqual(
  new Map([[1, null]]),
  new Map([['1', undefined]])
);
assertNotDeepOrStrict(
  new Map([[1, undefined]]),
  new Map([[2, undefined]])
);

// null as key
assertDeepAndStrictEqual(
  new Map([[null, 3]]),
  new Map([[null, 3]])
);
assertOnlyDeepEqual(
  new Map([[null, undefined]]),
  new Map([[undefined, null]])
);
assertOnlyDeepEqual(
  new Set([null]),
  new Set([undefined])
);

// GH-6416. Make sure circular refs don't throw.
{
  const b = {};
  b.b = b;
  const c = {};
  c.b = c;

  assertDeepAndStrictEqual(b, c);

  const d = {};
  d.a = 1;
  d.b = d;
  const e = {};
  e.a = 1;
  e.b = {};

  assertNotDeepOrStrict(d, e);
}

// GH-14441. Circular structures should be consistent
{
  const a = {};
  const b = {};
  a.a = a;
  b.a = {};
  b.a.a = a;
  assertDeepAndStrictEqual(a, b);
}

{
  const a = new Set();
  const b = new Set();
  const c = new Set();
  a.add(a);
  b.add(b);
  c.add(a);
  assertDeepAndStrictEqual(b, c);
}

// GH-7178. Ensure reflexivity of deepEqual with `arguments` objects.
{
  const args = (function() { return arguments; })();
  assertNotDeepOrStrict([], args);
}

// More checking that arguments objects are handled correctly
{
  // eslint-disable-next-line func-style
  const returnArguments = function() { return arguments; };

  const someArgs = returnArguments('a');
  const sameArgs = returnArguments('a');
  const diffArgs = returnArguments('b');

  assertNotDeepOrStrict(someArgs, ['a']);
  assertNotDeepOrStrict(someArgs, { '0': 'a' });
  assertNotDeepOrStrict(someArgs, diffArgs);
  assertDeepAndStrictEqual(someArgs, sameArgs);
}

{
  const values = [
    123,
    Infinity,
    0,
    null,
    undefined,
    false,
    true,
    {},
    [],
    () => {},
  ];
  assertDeepAndStrictEqual(new Set(values), new Set(values));
  assertDeepAndStrictEqual(new Set(values), new Set(values.reverse()));

  const mapValues = values.map((v) => [v, { a: 5 }]);
  assertDeepAndStrictEqual(new Map(mapValues), new Map(mapValues));
  assertDeepAndStrictEqual(new Map(mapValues), new Map(mapValues.reverse()));
}

{
  const s1 = new Set();
  const s2 = new Set();
  s1.add(1);
  s1.add(2);
  s2.add(2);
  s2.add(1);
  assertDeepAndStrictEqual(s1, s2);
}

{
  const m1 = new Map();
  const m2 = new Map();
  const obj = { a: 5, b: 6 };
  m1.set(1, obj);
  m1.set(2, 'hi');
  m1.set(3, [1, 2, 3]);

  m2.set(2, 'hi'); // different order
  m2.set(1, obj);
  m2.set(3, [1, 2, 3]); // deep equal, but not reference equal.

  assertDeepAndStrictEqual(m1, m2);
}

{
  const m1 = new Map();
  const m2 = new Map();

  // m1 contains itself.
  m1.set(1, m1);
  m2.set(1, new Map());

  assertNotDeepOrStrict(m1, m2);
}

{
  const map1 = new Map([[1, 1]]);
  const map2 = new Map([[1, '1']]);
  assert.deepEqual(map1, map2);
  assert.throws(() => assert.deepStrictEqual(map1, map2),
                re`${map1} deepStrictEqual ${map2}`);
}

{
  // Two equivalent sets / maps with different key/values applied shouldn't be
  // the same. This is a terrible idea to do in practice, but deepEqual should
  // still check for it.
  const s1 = new Set();
  const s2 = new Set();
  s1.x = 5;
  assertNotDeepOrStrict(s1, s2);

  const m1 = new Map();
  const m2 = new Map();
  m1.x = 5;
  assertNotDeepOrStrict(m1, m2);
}

{
  // Circular references.
  const s1 = new Set();
  s1.add(s1);
  const s2 = new Set();
  s2.add(s2);
  assertDeepAndStrictEqual(s1, s2);

  const m1 = new Map();
  m1.set(2, m1);
  const m2 = new Map();
  m2.set(2, m2);
  assertDeepAndStrictEqual(m1, m2);

  const m3 = new Map();
  m3.set(m3, 2);
  const m4 = new Map();
  m4.set(m4, 2);
  assertDeepAndStrictEqual(m3, m4);
}

// Handle sparse arrays
assertDeepAndStrictEqual([1, , , 3], [1, , , 3]);
assertOnlyDeepEqual([1, , , 3], [1, , , 3, , , ]);

// Handle different error messages
{
  const err1 = new Error('foo1');
  const err2 = new Error('foo2');
  const err3 = new TypeError('foo1');
  assertNotDeepOrStrict(err1, err2, assert.AssertionError);
  assertNotDeepOrStrict(err1, err3, assert.AssertionError);
  // TODO: evaluate if this should throw or not. The same applies for RegExp
  // Date and any object that has the same keys but not the same prototype.
  assertOnlyDeepEqual(err1, {}, assert.AssertionError);
}

// Handle NaN
assert.throws(() => { assert.deepEqual(NaN, NaN); }, assert.AssertionError);
assert.doesNotThrow(() => { assert.deepStrictEqual(NaN, NaN); });
assert.doesNotThrow(() => { assert.deepStrictEqual({ a: NaN }, { a: NaN }); });
assert.doesNotThrow(
  () => { assert.deepStrictEqual([ 1, 2, NaN, 4 ], [ 1, 2, NaN, 4 ]); });

// Handle boxed primitives
{
  const boxedString = new String('test');
  const boxedSymbol = Object(Symbol());
  assertOnlyDeepEqual(new Boolean(true), Object(false));
  assertOnlyDeepEqual(Object(true), new Number(1));
  assertOnlyDeepEqual(new Number(2), new Number(1));
  assertOnlyDeepEqual(boxedSymbol, Object(Symbol()));
  assertOnlyDeepEqual(boxedSymbol, {});
  assertDeepAndStrictEqual(boxedSymbol, boxedSymbol);
  assertDeepAndStrictEqual(Object(true), Object(true));
  assertDeepAndStrictEqual(Object(2), Object(2));
  assertDeepAndStrictEqual(boxedString, Object('test'));
  boxedString.slow = true;
  assertNotDeepOrStrict(boxedString, Object('test'));
  boxedSymbol.slow = true;
  assertNotDeepOrStrict(boxedSymbol, {});
}

// Minus zero
assertOnlyDeepEqual(0, -0);
assertDeepAndStrictEqual(-0, -0);

// Handle symbols (enumerable only)
{
  const symbol1 = Symbol();
  const obj1 = { [symbol1]: 1 };
  const obj2 = { [symbol1]: 1 };
  const obj3 = { [Symbol()]: 1 };
  // Add a non enumerable symbol as well. It is going to be ignored!
  Object.defineProperty(obj2, Symbol(), { value: 1 });
  assertOnlyDeepEqual(obj1, obj3);
  assertDeepAndStrictEqual(obj1, obj2);
  // TypedArrays have a fast path. Test for this as well.
  const a = new Uint8Array(4);
  const b = new Uint8Array(4);
  a[symbol1] = true;
  b[symbol1] = false;
  assertOnlyDeepEqual(a, b);
  b[symbol1] = true;
  assertDeepAndStrictEqual(a, b);
  // The same as TypedArrays is valid for boxed primitives
  const boxedStringA = new String('test');
  const boxedStringB = new String('test');
  boxedStringA[symbol1] = true;
  assertOnlyDeepEqual(boxedStringA, boxedStringB);
  boxedStringA[symbol1] = true;
  assertDeepAndStrictEqual(a, b);
}

/* eslint-enable */
