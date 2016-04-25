'use strict';
var common = require('../common.js');
var crypto = require('crypto');
var assert = require('assert');

const big = crypto.randomBytes(100000000 / 2).toString('hex'); // 100mb
const bigAssert = 100 * 1000 * 1000;
const small = '4320abc78a25ff6d9afbcb51924dcb4eef8ac424a0d7451b6e5001528c8a46' +
              '5decff7d8e82e26176769b4266fdc05e4773bf'; // 100byte
const smallAssert = '4320abc78a25ff6d9afbCB51924dCB4eef8ac424a0d7451b6e500152' +
                    '8c8a465decff7d8e82e26176769b4266fdc05e4773bf'; // 100byte

var bench = common.createBenchmark(main, {
  size: ['small', 'big'],
  assert: ['true', 'false'],
  type: ['js_native', 'string'],
  n: [1024]
});

var smallBuf = new Buffer(small);
var bigBuf = new Buffer(big);
var findStr = 'cb';
var find = new Buffer(findStr);
var replaceStr = 'CB';
var replace = new Buffer(replaceStr);
var splitAndJoin = null;


function main(conf) {
  let buf = null;
  let len = null;
  var n = +conf.n;

  switch (conf.size) {
    case 'small':
      buf = smallBuf;
      len = n * 1024;
      break;
    case 'big':
      buf = bigBuf;
      len = 10;
      break;
    default:
      buf = smallBuf;
      len = n;
  }
  switch (conf.type) {
    case 'string':
      splitAndJoin = function(buf) {
        const res = buf.toString().split(findStr);
        return res.join(replaceStr);
      };
      break;
    case 'js_native':
      splitAndJoin = function(buf) {
        const res = buf.split(find);
        return Buffer.join(res, replace);
      };
      break;
  }

  bench.start();
  let res = null;
  for (var i = 0; i < len; i++) {
    res = splitAndJoin(buf);
  }

  if (conf.assert === 'true' && conf.size === 'small') {
    assert(res.toString() === smallAssert);
  }

  if (conf.assert === 'true' && conf.size === 'big') {
    assert(res.toString().length === bigAssert);
  }
  res = null;
  buf = null;
  len = null;
  bench.end(n);
}
