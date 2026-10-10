// Flags: --allow-natives-syntax
'use strict';
const common = require('../../common');
const assert = require('assert');

// node_api_create_object_with_properties() returns objects with fast
// properties and a shared map once it has seen the same names before.

const { CreateObjectWithProperties: create } =
  require(`./build/${common.buildType}/test_object`);

function assertSharedShape(objects) {
  // eslint-disable-next-line no-unused-vars
  for (const object of objects) {
    assert(eval('%HasFastProperties(object)'));
    assert(eval('%HaveSameMap(object, objects[0])'));
  }
}

function dataProperty(value) {
  return { value, writable: true, enumerable: true, configurable: true };
}

{
  const names = ['id', 'name', 'score'];
  const prototype = { greet() { return 'hi'; } };
  for (const [argument, expected] of [
    [undefined, null],
    [null, null],
    [Object.prototype, Object.prototype],
    [prototype, prototype],
  ]) {
    const objects = [];
    for (let i = 0; i < 5; i++) {
      objects.push(create(names, [i, `row ${i}`, i * 1.5], argument));
    }
    for (const [i, object] of objects.entries()) {
      assert.strictEqual(Object.getPrototypeOf(object), expected);
      assert.deepStrictEqual(Object.getOwnPropertyDescriptors(object), {
        id: dataProperty(i),
        name: dataProperty(`row ${i}`),
        score: dataProperty(i * 1.5),
      });
    }
    assertSharedShape(objects.slice(1));
  }
}

// Names are matched by content, not identity.
{
  const objects = [];
  for (let i = 0; i < 3; i++) {
    const names = ['x', 'y'].map((name) => name.split('').join(''));
    objects.push(create(names, [i, i], undefined));
  }
  assertSharedShape(objects.slice(1));
}

// One-byte names outside ASCII.
{
  const objects = [];
  for (let i = 0; i < 3; i++) {
    objects.push(create(['café', 'ü'], [i, i], undefined));
  }
  assert.deepStrictEqual(Object.keys(objects[2]), ['café', 'ü']);
  assertSharedShape(objects.slice(1));
}

// Up to 127 properties.
{
  const names = Array.from({ length: 127 }, (_, i) => `p${i}`);
  const objects = [];
  for (let i = 0; i < 3; i++) objects.push(create(names, names, undefined));
  assert.deepStrictEqual(Object.values(objects[2]), names);
  assertSharedShape(objects.slice(1));
}

// Names that V8 cannot take in a template keep their behavior: a symbol, an
// array index, a two-byte string, a duplicate (the last value wins), and
// more than 127 names.
{
  const symbol = Symbol('s');
  const many = Array.from({ length: 128 }, (_, i) => `p${i}`);
  for (const [names, values, expected] of [
    [[symbol, 'a'], [1, 2], { [symbol]: 1, a: 2 }],
    [['0', 'a'], [1, 2], { 0: 1, a: 2 }],
    [['名前', 'a'], [1, 2], { '名前': 1, 'a': 2 }],
    [['a', 'b', 'a'], [1, 2, 3], { a: 3, b: 2 }],
    [many, many, Object.fromEntries(many.map((name) => [name, name]))],
  ]) {
    for (let i = 0; i < 3; i++) {
      const object = create(names, values, undefined);
      assert.deepStrictEqual(object, { __proto__: null, ...expected });
      assert.deepStrictEqual(Reflect.ownKeys(object), Reflect.ownKeys(expected));
    }
  }
}

// Every list of names passed again gets fast objects, not only some of them.
for (let i = 0; i < 100; i++) {
  const objects = [];
  for (let j = 0; j < 3; j++) {
    objects.push(create([`c${i}`, `d${i}`], [i, j], undefined));
  }
  assertSharedShape(objects.slice(1));
}

// More lists of names than the cache has entries.
for (let round = 0; round < 3; round++) {
  for (let i = 0; i < 1000; i++) {
    const object = create([`a${i}`, `b${i}`], [i, round], undefined);
    assert.deepStrictEqual(object, {
      __proto__: null,
      [`a${i}`]: i,
      [`b${i}`]: round,
    });
  }
}
