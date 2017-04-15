'use strict';

const common = require('../common.js');
const assert = require('assert');
const Writable = require('stream').Writable;
const util = require('util');

const methods = [
  'restAndSpread',
  'argumentsAndApply',
  'restAndApply',
  'restAndConcat'
];

var bench = common.createBenchmark(main, {
  method: methods,
  concat: [1, 0],
  n: [1000000]
});

const nullStream = createNullStream();

function usingRestAndConcat(...args) {
  nullStream.write('this is ' + args[0] + ' of ' + args[1] + '\n');
}

function usingRestAndSpreadTS(...args) {
  nullStream.write(`${util.format(...args)}\n`);
}

function usingRestAndApplyTS(...args) {
  nullStream.write(`${util.format.apply(null, args)}\n`);
}

function usingArgumentsAndApplyTS() {
  nullStream.write(`${util.format.apply(null, arguments)}\n`);
}

function usingRestAndSpreadC(...args) {
  nullStream.write(util.format(...args) + '\n');
}

function usingRestAndApplyC(...args) {
  nullStream.write(util.format.apply(null, args) + '\n');
}

function usingArgumentsAndApplyC() {
  nullStream.write(util.format.apply(null, arguments) + '\n');
}

function runUsingRestAndConcat(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    usingRestAndConcat('a', 1);
  bench.end(n);
}

function runUsingRestAndSpread(n, concat) {

  const method = concat ? usingRestAndSpreadC : usingRestAndSpreadTS;

  var i = 0;
  bench.start();
  for (; i < n; i++)
    method('this is %s of %d', 'a', 1);
  bench.end(n);
}

function runUsingRestAndApply(n, concat) {

  const method = concat ? usingRestAndApplyC : usingRestAndApplyTS;

  var i = 0;
  bench.start();
  for (; i < n; i++)
    method('this is %s of %d', 'a', 1);
  bench.end(n);
}

function runUsingArgumentsAndApply(n, concat) {

  const method = concat ? usingArgumentsAndApplyC : usingArgumentsAndApplyTS;

  var i = 0;
  bench.start();
  for (; i < n; i++)
    method('this is %s of %d', 'a', 1);
  bench.end(n);
}

function main(conf) {
  const n = +conf.n;
  switch (conf.method) {
    case 'restAndSpread':
      runUsingRestAndSpread(n, conf.concat);
      break;
    case 'restAndApply':
      runUsingRestAndApply(n, conf.concat);
      break;
    case 'argumentsAndApply':
      runUsingArgumentsAndApply(n, conf.concat);
      break;
    case 'restAndConcat':
      if (conf.concat)
        runUsingRestAndConcat(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}

function createNullStream() {
  // Used to approximate /dev/null
  function NullStream() {
    Writable.call(this, {});
  }
  util.inherits(NullStream, Writable);
  NullStream.prototype._write = function(cb) {
    assert.strictEqual(cb.toString(), 'this is a of 1\n');
  };
  return new NullStream();
}
