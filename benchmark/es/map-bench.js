'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['object', 'nullProtoObject', 'fakeMap', 'map'],
  millions: [10]
});

function runObject(n) {
  const m = {};
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m['i' + n] = n;
    m['s' + n] = String(n);
    assert.equal(m['i' + n], m['s' + n]);
    m['i' + n] = undefined;
    m['s' + n] = undefined;
  }
  bench.end(n / 1e6);
}

function runNullProtoObject(n) {
  const m = Object.create(null);
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m['i' + n] = n;
    m['s' + n] = String(n);
    assert.equal(m['i' + n], m['s' + n]);
    m['i' + n] = undefined;
    m['s' + n] = undefined;
  }
  bench.end(n / 1e6);
}

function fakeMap() {
  const m = {};
  return {
    get(key) { return m['$' + key]; },
    set(key, val) { m['$' + key] = val; },
    get size() { return Object.keys(m).length; },
    has(key) { return Object.prototype.hasOwnProperty.call(m, '$' + key); }
  };
}

function runFakeMap(n) {
  const m = fakeMap();
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m.set('i' + n, n);
    m.set('s' + n, String(n));
    assert.equal(m.get('i' + n), m.get('s' + n));
    m.set('i' + n, undefined);
    m.set('s' + n, undefined);
  }
  bench.end(n / 1e6);
}

function runMap(n) {
  const m = new Map();
  var i = 0;
  bench.start();
  for (; i < n; i++) {
    m.set('i' + n, n);
    m.set('s' + n, String(n));
    assert.equal(m.get('i' + n), m.get('s' + n));
    m.set('i' + n, undefined);
    m.set('s' + n, undefined);
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
