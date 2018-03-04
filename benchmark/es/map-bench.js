'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: [
    'object', 'nullProtoObject', 'nullProtoLiteralObject', 'storageObject',
    'fakeMap', 'map',
  ],
  millions: [1],
});

function runObject(millions) {
  const m = {};
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(millions);
}

function runNullProtoObject(millions) {
  const m = Object.create(null);
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(millions);
}

function runNullProtoLiteralObject(millions) {
  const m = { __proto__: null };
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(millions);
}

function StorageObject() {}
StorageObject.prototype = Object.create(null);

function runStorageObject(millions) {
  const m = new StorageObject();
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(millions);
}

function fakeMap() {
  const m = {};
  return {
    get(key) { return m[`$${key}`]; },
    set(key, val) { m[`$${key}`] = val; },
    get size() { return Object.keys(m).length; },
    has(key) { return Object.prototype.hasOwnProperty.call(m, `$${key}`); },
  };
}

function runFakeMap(millions) {
  const m = fakeMap();
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(millions);
}

function runMap(millions) {
  const m = new Map();
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(millions);
}

function main({ millions, method }) {
  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'object':
      runObject(millions);
      break;
    case 'nullProtoObject':
      runNullProtoObject(millions);
      break;
    case 'nullProtoLiteralObject':
      runNullProtoLiteralObject(millions);
      break;
    case 'storageObject':
      runStorageObject(millions);
      break;
    case 'fakeMap':
      runFakeMap(millions);
      break;
    case 'map':
      runMap(millions);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
