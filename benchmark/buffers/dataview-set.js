'use strict';
var common = require('../common.js');

var types = [
  'Uint8',
  'Uint16LE',
  'Uint16BE',
  'Uint32LE',
  'Uint32BE',
  'Int8',
  'Int16LE',
  'Int16BE',
  'Int32LE',
  'Int32BE',
  'Float32LE',
  'Float32BE',
  'Float64LE',
  'Float64BE'
];

var bench = common.createBenchmark(main, {
  type: types,
  millions: [1]
});

const INT8 = 0x7f;
const INT16 = 0x7fff;
const INT32 = 0x7fffffff;
const UINT8 = INT8 * 2;
const UINT16 = INT16 * 2;
const UINT32 = INT32 * 2;

var mod = {
  setInt8: INT8,
  setInt16: INT16,
  setInt32: INT32,
  setUint8: UINT8,
  setUint16: UINT16,
  setUint32: UINT32
};

function main(conf) {
  var len = +conf.millions * 1e6;
  var ab = new ArrayBuffer(8);
  var dv = new DataView(ab, 0, 8);
  var le = /LE$/.test(conf.type);
  var fn = 'set' + conf.type.replace(/[LB]E$/, '');

  if (/int/i.test(fn))
    benchInt(dv, fn, len, le);
  else
    benchFloat(dv, fn, len, le);
}

function benchInt(dv, fn, len, le) {
  const m = mod[fn];
  const method = dv[fn];
  bench.start();
  for (var i = 0; i < len; i++) {
    method.call(dv, 0, i % m, le);
  }
  bench.end(len / 1e6);
}

function benchFloat(dv, fn, len, le) {
  const method = dv[fn];
  bench.start();
  for (var i = 0; i < len; i++) {
    method.call(dv, 0, i * 0.1, le);
  }
  bench.end(len / 1e6);
}
