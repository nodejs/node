'use strict';
const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  method: ['encode', 'tostring'],
  enc: ['utf8', 'utf16', 'hex', 'base64'],
  size: [16, 512, 1024, 4096],
  millions: [1]
});

function encode(str, target, iter) {
  Buffer.encode(str, target);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(Buffer.encode)');
  Buffer.encode(str, target);
  doencode(str, target, iter);
}

function doencode(str, target, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    Buffer.encode(str, target);
  bench.end(iter / 1e6);
}

function tostring(str, target, iter) {
  Buffer.from(str).toString(target);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(Buffer.from)');
  eval('%OptimizeFunctionOnNextCall(Buffer.prototype.toString)');
  Buffer.from(str).toString(target);
  dotostring(str, target, iter);
}

function dotostring(str, target, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    Buffer.from(str).toString(target);
  bench.end(iter / 1e6);
}

function main(conf) {
  const iter = (conf.millions >>> 0) * 1e6;
  const enc = conf.env;
  const size = (conf.size >>> 0);
  const method = conf.method === 'encode' ?
      encode : tostring;
  const str = Buffer.alloc(size, 'a').toString();
  method(str, enc, iter);
}
