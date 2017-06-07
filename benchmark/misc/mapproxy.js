'use strict';

const common = require('../common.js');
const mp = require('internal/mapproxy');
const assert = require('assert');

var bench = common.createBenchmark(main, {
  n: [100000],
  m: ['nullproto', 'map', 'mapproxy']
});

function runNullProtoObject() {
  const obj = Object.create(null);
  obj.a = 'testing';
  obj.b = 'this';
  obj.c = 'thing';
  delete obj.a;
  delete obj.b;
  assert.strictEqual(obj.c, 'thing');
}

function runMap() {
  const map = new Map();
  map.set('a', 'testing');
  map.set('b', 'this');
  map.set('c', 'thing');
  map.delete('a');
  map.delete('b');
  assert.strictEqual(map.get('c'), 'thing');
}

function runMapProxy() {
  const map = new Map();
  const obj = mp.wrap(map);
  obj.a = 'testing';
  obj.b = 'this';
  obj.c = 'thing';
  delete obj.a;
  delete obj.b;
  assert.strictEqual(obj.c, 'thing');
}

function doNullProtoObject(n) {
  common.v8ForceOptimization(runNullProtoObject);
  bench.start();
  for (var i = 0; i < n; i++)
    runNullProtoObject();
  bench.end(n);
}

function doMap(n) {
  common.v8ForceOptimization(runMap);
  bench.start();
  for (var i = 0; i < n; i++) {
    runMap();
  }
  bench.end(n);
}

function doMapProxy(n) {
  common.v8ForceOptimization(runMapProxy);
  bench.start();
  for (var i = 0; i < n; i++)
    runMapProxy();
  bench.end(n);
}

function main(conf) {
  const n = conf.n;
  const m = conf.m;
  console.log(n);
  switch (m) {
    case 'nullproto':
      return doNullProtoObject(n);
    case 'map':
      return doMap(n);
    case 'mapproxy':
      return doMapProxy(n);
    default:
      throw new Error('unexpected parameter ' + m);
  }
}
