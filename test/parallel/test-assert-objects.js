'use strict';

require('../common');
const vm = require('node:vm');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { KeyObject } = require('node:crypto');
const { subtle } = globalThis.crypto;

// Helper functions for testing
function createCircularObject() {
  const obj = {};
  obj.self = obj;
  return obj;
}

function createDeepNestedObject() {
  return { level1: { level2: { level3: 'deepValue' } } };
}


// Test cases
describe('Object Comparison Tests', function() {
  it('should strictly compare two non identical simple objects', function() {
    const obj1 = { a: 1, b: 'foo', c: 'bar' };
    const obj2 = { a: 1, c: 'bar' };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should loosely compare two non identical simple objects', function() {
    const obj1 = { a: 1, b: 'foo', c: '1' };
    const obj2 = { a: 1, c: 1 };
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two identical simple objects', function() {
    const obj1 = { a: 1, b: 'string' };
    const obj2 = { a: 1, b: 'string' };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two different simple objects', function() {
    const obj1 = { a: 1, b: 'string' };
    const obj2 = { a: 2, b: 'string' };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should loosely compare two similar objects with type coercion', function() {
    const obj1 = { a: 1, b: '2' };
    const obj2 = { a: 1, b: 2 };
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two objects with different property order', function() {
    const obj1 = { a: 1, b: 'string' };
    const obj2 = { b: 'string', a: 1 };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should strictly compare two objects with nested objects', function() {
    const obj1 = createDeepNestedObject();
    const obj2 = createDeepNestedObject();
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different nested objects', function() {
    const obj1 = createDeepNestedObject();
    const obj2 = { level1: { level2: { level3: 'differentValue' } } };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should loosely compare two objects with nested objects and type coercion', function() {
    const obj1 = { level1: { level2: { level3: '42' } } };
    const obj2 = { level1: { level2: { level3: 42 } } };
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two objects with circular references', function() {
    const obj1 = createCircularObject();
    const obj2 = createCircularObject();
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should strictly compare two objects with different circular references', function() {
    const obj1 = createCircularObject();
    const obj2 = { self: {} };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should loosely compare two objects with circular references', function() {
    const obj1 = createCircularObject();
    const obj2 = createCircularObject();
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two arrays with identical elements', function() {
    const arr1 = [1, 'two', true];
    const arr2 = [1, 'two', true];
    assert.matchObjectStrict(arr1, arr2);
  });

  it('should not strictly compare two arrays with different elements', function() {
    const arr1 = [1, 'two', true];
    const arr2 = [1, 'two', false];
    assert.throws(() => assert.matchObjectStrict(arr1, arr2), Error);
  });

  it('should loosely compare two arrays with type coercion', function() {
    const arr1 = [1, '2', true];
    const arr2 = [1, 2, 1];
    assert.matchObject(arr1, arr2);
  });

  it('should strictly compare two Date objects with the same time', function() {
    const date1 = new Date(0);
    const date2 = new Date(0);
    assert.matchObjectStrict(date1, date2);
  });

  it('should not strictly compare two Date objects with different times', function() {
    const date1 = new Date(0);
    const date2 = new Date(1);
    assert.throws(() => assert.matchObjectStrict(date1, date2), Error);
  });

  it('should strictly compare two objects with large number of properties', function() {
    const obj1 = Object.fromEntries(Array.from({ length: 100 }, (_, i) => [`key${i}`, i]));
    const obj2 = Object.fromEntries(Array.from({ length: 100 }, (_, i) => [`key${i}`, i]));
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different large number of properties', function() {
    const obj1 = Object.fromEntries(Array.from({ length: 100 }, (_, i) => [`key${i}`, i]));
    const obj2 = Object.fromEntries(Array.from({ length: 100 }, (_, i) => [`key${i}`, i + 1]));
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should strictly compare two objects with Symbol properties', function() {
    const sym = Symbol('test');
    const obj1 = { [sym]: 'symbol' };
    const obj2 = { [sym]: 'symbol' };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different Symbols', function() {
    const sym1 = Symbol('test');
    const sym2 = Symbol('test');
    const obj1 = { [sym1]: 'symbol' };
    const obj2 = { [sym2]: 'symbol' };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should loosely compare two objects with numeric string and number', function() {
    const obj1 = { a: '100' };
    const obj2 = { a: 100 };
    assert.matchObject(obj1, obj2);
  });

  it('should not strictly compare two objects with different array properties', function() {
    const obj1 = { a: [1, 2, 3] };
    const obj2 = { a: [1, 2, 4] };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should loosely compare two objects with boolean and numeric representations', function() {
    const obj1 = { a: 1, b: 0 };
    const obj2 = { a: true, b: false };
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two objects with RegExp properties', function() {
    const obj1 = { pattern: /abc/ };
    const obj2 = { pattern: /abc/ };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different RegExp properties', function() {
    const obj1 = { pattern: /abc/ };
    const obj2 = { pattern: /def/ };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should not loosely compare two objects with null and empty object', function() {
    const obj1 = { a: null };
    const obj2 = { a: {} };
    assert.throws(() => assert.matchObject(obj1, obj2), Error);
  });

  it('should strictly compare two objects with identical function properties', function() {
    const func = () => {};
    const obj1 = { fn: func };
    const obj2 = { fn: func };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different function properties', function() {
    const obj1 = { fn: () => {} };
    const obj2 = { fn: () => {} };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should loosely compare two objects with undefined and missing properties', function() {
    const obj1 = { a: undefined };
    const obj2 = {};
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two objects with mixed types of properties', function() {
    const sym = Symbol('test');
    const obj1 = { num: 1, str: 'test', bool: true, sym: sym };
    const obj2 = { num: 1, str: 'test', bool: true, sym: sym };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should compare two objects with Buffers', function() {
    const obj1 = { buf: Buffer.from('Node.js') };
    const obj2 = { buf: Buffer.from('Node.js') };
    assert.matchObjectStrict(obj1, obj2);
    assert.matchObject(obj1, obj2);
  });

  it('should strictly compare two objects with identical Error properties', function() {
    const error = new Error('Test error');
    const obj1 = { error: error };
    const obj2 = { error: error };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different Error instances', function() {
    const obj1 = { error: new Error('Test error 1') };
    const obj2 = { error: new Error('Test error 2') };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should strictly compare two objects with the same TypedArray instance', function() {
    const typedArray = new Uint8Array([1, 2, 3]);
    const obj1 = { typedArray: typedArray };
    const obj2 = { typedArray: typedArray };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should not strictly compare two objects with different TypedArray instances and content', function() {
    const obj1 = { typedArray: new Uint8Array([1, 2, 3]) };
    const obj2 = { typedArray: new Uint8Array([4, 5, 6]) };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should not strictly compare two objects with different TypedArray instances', function() {
    const obj1 = { typedArray: new Uint8Array([1, 2, 3]) };
    const obj2 = { typedArray: new Uint8Array([1, 2, 3]) };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should strictly compare two Map objects with identical entries', function() {
    const map1 = new Map([
      ['key1', 'value1'],
      ['key2', 'value2'],
    ]);
    const map2 = new Map([
      ['key1', 'value1'],
      ['key2', 'value2'],
    ]);
    assert.matchObjectStrict(map1, map2);
  });

  it('should not strictly compare two Map objects with different entries', function() {
    const map1 = new Map([
      ['key1', 'value1'],
      ['key2', 'value2'],
    ]);
    const map2 = new Map([
      ['key1', 'value1'],
      ['key3', 'value3'],
    ]);
    assert.throws(() => assert.matchObjectStrict(map1, map2), Error);
  });

  it('should strictly compare two Set objects with identical values', function() {
    const set1 = new Set(['value1', 'value2']);
    const set2 = new Set(['value1', 'value2']);
    assert.matchObjectStrict(set1, set2);
  });

  it('should strictly compare two Map objects from different realms with identical entries', function() {
    const map1 = new vm.runInNewContext('new Map([["key1", "value1"], ["key2", "value2"]])');
    const map2 = new Map([
      ['key1', 'value1'],
      ['key2', 'value2'],
    ]);
    assert.matchObjectStrict(map1, map2);
  });

  it('should not strictly compare two Set objects from different realms with different values', function() {
    const set1 = new vm.runInNewContext('new Set(["value1", "value2"])');
    const set2 = new Set(['value1', 'value3']);
    assert.throws(() => assert.matchObjectStrict(set1, set2), Error);
  });

  it('should not strictly compare two Set objects with different values', function() {
    const set1 = new Set(['value1', 'value2']);
    const set2 = new Set(['value1', 'value3']);
    assert.throws(() => assert.matchObjectStrict(set1, set2), Error);
  });

  it('should compare plain objects from different realms', function() {
    const obj1 = vm.runInNewContext(`({
        a: 1,
        b: 2n,
        c: "3",
        d: /4/,
        e: new Set([5]),
        f: [6],
        g: new Uint8Array()
      })`);
    const obj2 = { b: 2n, e: new Set([5]), f: [6], g: new Uint8Array() };
    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should strictly compare two objects with identical getter/setter properties', function() {
    const createObjectWithGetterSetter = () => {
      let value = 'test';
      return Object.defineProperty({}, 'prop', {
        get: () => value,
        set: (newValue) => {
          value = newValue;
        },
        enumerable: true,
        configurable: true,
      });
    };

    const obj1 = createObjectWithGetterSetter();
    const obj2 = createObjectWithGetterSetter();
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should strictly compare two objects with no prototype', function() {
    const obj1 = { __proto__: null, prop: 'value' };
    const obj2 = { __proto__: null, prop: 'value' };
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should strictly compare two objects with identical non-enumerable properties', function() {
    const obj1 = {};
    Object.defineProperty(obj1, 'hidden', {
      value: 'secret',
      enumerable: false,
    });
    const obj2 = {};
    Object.defineProperty(obj2, 'hidden', {
      value: 'secret',
      enumerable: false,
    });
    assert.matchObjectStrict(obj1, obj2);
  });

  it('should compare two identical primitives', function() {
    ['foo', 1, 1n, false, null, undefined, Symbol()].forEach((val) => {
      assert.matchObject(val, val);
      assert.matchObjectStrict(val, val);
    });
  });

  it('should compare two objects with different CryptoKey instances objects', async function() {
    const { cryptoKey: cryptoKey1 } = await generateCryptoKey();
    const { cryptoKey: cryptoKey2 } = await generateCryptoKey();

    const obj1 = { cryptoKey: cryptoKey1 };
    const obj2 = { cryptoKey: cryptoKey2 };

    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });

  it('should compare two objects with different KeyObject objects', async function() {
    const { keyObject: keyObject1 } = await generateCryptoKey();
    const { keyObject: keyObject2 } = await generateCryptoKey();

    const obj1 = { keyObject: keyObject1 };
    const obj2 = { keyObject: keyObject2 };

    assert.throws(() => assert.matchObjectStrict(obj1, obj2), Error);
  });
});

async function generateCryptoKey() {
  const cryptoKey = await subtle.generateKey({
    name: 'HMAC',
    hash: 'SHA-256',
    length: 256,
  }, true, ['sign', 'verify']);

  const keyObject = KeyObject.from(cryptoKey);

  return { cryptoKey, keyObject };
}
