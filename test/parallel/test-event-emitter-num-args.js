'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

const e = new events.EventEmitter();
const num_args_emitted = [];

e.on('numArgs', function() {
  const numArgs = arguments.length;
  num_args_emitted.push(numArgs);
});

e.on('foo', function() {
  num_args_emitted.push(arguments.length);
});

e.on('foo', function() {
  num_args_emitted.push(arguments.length);
});

e.emit('numArgs');
e.emit('numArgs', null);
e.emit('numArgs', null, null);
e.emit('numArgs', null, null, null);
e.emit('numArgs', null, null, null, null);
e.emit('numArgs', null, null, null, null, null);

e.emit('foo', null, null, null, null);

process.on('exit', function() {
  assert.deepStrictEqual([0, 1, 2, 3, 4, 5, 4, 4], num_args_emitted);
});
