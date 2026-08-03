'use strict';
require('../../common');
Error.stackTraceLimit = 2;

const EventEmitter = require('events');

function foo() {
  function bar() {
    return new Error('foo:bar');
  }

  return bar();
}

const ee = new EventEmitter();
const err = foo();

function quux() {
  ee.emit('error', err);
}

quux();
