'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

// const keepAlive = setInterval(() => {}, 9999);
var cbCalls = 0;
const expectedCalls = 2;

const cb = common.mustCall((data) => {
  console.log('foo');
  assert(data instanceof Buffer);
  // if (++cbCalls === expectedCalls) {
  //   clearInterval(keepAlive);
  // }
}, expectedCalls);

const command = common.isWindows ? 'dir' : 'ls';
exec(command).stdout.on('data', cb);

exec('fhqwhgads').stderr.on('data', cb);
