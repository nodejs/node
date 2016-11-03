'use strict';

require('../common');

const assert = require('assert');
const Writable = require('stream').Writable;

const stream = new Writable({ write });

let errorsRecorded = 0;

function write() {
  throw new Error('write() should not have been called!');
}

function errorRecorder(err) {
  if (err.message === 'write after end') {
    errorsRecorded++;
  }
}

// should trigger ended errors when writing later
stream.end();

stream.on('error', errorRecorder);
stream.write('this should explode', errorRecorder);

assert.equal(errorsRecorded, 0,
  'Waits until next tick before emitting error');

process.nextTick(() => assert.equal(errorsRecorded, 2));
