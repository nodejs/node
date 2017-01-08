'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

const e = new events.EventEmitter();
const num_args_emited = [];

e.on('numArgs', function() {
  const numArgs = arguments.length;
  console.log('numArgs: ' + numArgs);
  num_args_emited.push(numArgs);
});

console.log('start');

e.emit('numArgs');
e.emit('numArgs', null);
e.emit('numArgs', null, null);
e.emit('numArgs', null, null, null);
e.emit('numArgs', null, null, null, null);
e.emit('numArgs', null, null, null, null, null);

process.on('exit', function() {
  assert.deepStrictEqual([0, 1, 2, 3, 4, 5], num_args_emited);
});
