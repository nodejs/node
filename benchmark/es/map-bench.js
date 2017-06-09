'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: [
    'object', 'nullProtoObject', 'nullProtoLiteralObject', 'storageObject',
    'fakeMap', 'map'
  ],
  millions: [1]
});

function runObject(n) {
  const m = {};
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n / 1e6);
}

function runNullProtoObject(n) {
  const m = Object.create(null);
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n / 1e6);
}

function runNullProtoLiteralObject(n) {
  const m = { __proto__: null };
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n / 1e6);
}

function StorageObject() {}
StorageObject.prototype = Object.create(null);

function runStorageObject(n) {
  const m = new StorageObject();
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m[`i${i}`] = i;
    m[`s${i}`] = String(i);
    assert.strictEqual(String(m[`i${i}`]), m[`s${i}`]);
    m[`i${i}`] = undefined;
    m[`s${i}`] = undefined;
  }
  bench.end(n / 1e6);
}

function fakeMap() {
  const m = {};
  return {
    get(key) { return m[`$${key}`]; },
    set(key, val) { m[`$${key}`] = val; },
    get size() { return Object.keys(m).length; },
    has(key) { return Object.prototype.hasOwnProperty.call(m, `$${key}`); }
  };
}

function runFakeMap(n) {
  const m = fakeMap();
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(n / 1e6);
}

function runMap(n) {
  const m = new Map();
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m.set(`i${i}`, i);
    m.set(`s${i}`, String(i));
    assert.strictEqual(String(m.get(`i${i}`)), m.get(`s${i}`));
    m.set(`i${i}`, undefined);
    m.set(`s${i}`, undefined);
  }
  bench.end(n / 1e6);
}

function main(conf) {
  const n = +conf.millions * 1e6;

  switch (conf.method) {
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
      throw new Error('Unexpected method');
  }
}
