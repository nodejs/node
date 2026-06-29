'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: [
    'object', 'nullProtoObject', 'nullProtoLiteralObject', 'storageObject',
    'fakeMap', 'map',
  ],
  n: [1e6],
});

function runObject(n) {
  const m = {};
  bench.start();
  for (let i = 0; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n);
}

function runNullProtoObject(n) {
  const m = { __proto__: null };
  bench.start();
  for (let i = 0; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n);
}

function runNullProtoLiteralObject(n) {
  const m = { __proto__: null };
  bench.start();
  for (let i = 0; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n);
}

function StorageObject() {}
StorageObject.prototype = { __proto__: null };

function runStorageObject(n) {
  const m = new StorageObject();
  bench.start();
  for (let i = 0; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n);
}

function fakeMap() {
  const m = {};
  return {
    get(key) { return m[`$${key}`]; },
    set(key, val) { m[`$${key}`] = val; },
    get size() { return Object.keys(m).length; },
    has(key) { return Object.hasOwn(m, `$${key}`); },
  };
}

function runFakeMap(n) {
  const m = fakeMap();
  bench.start();
  for (let i = 0; i < n; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(n);
}

function runMap(n) {
  const m = new Map();
  bench.start();
  for (let i = 0; i < n; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'object':
      runObject(n);
      break;
    case 'nullProtoObject':
      runNullProtoObject(n);
      break;
    case 'nullProtoLiteralObject':
      runNullProtoLiteralObject(n);
      break;
    case 'storageObject':
      runStorageObject(n);
      break;
    case 'fakeMap':
      runFakeMap(n);
      break;
    case 'map':
      runMap(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
