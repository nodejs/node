'use strict';
require('../common');
const { Writable } = require('stream');
const { Console } = require('console');

const stream = new Writable({
  write() {
    throw null; // eslint-disable-line no-throw-literal
  }
});

const console = new Console({ stdout: stream });

console.log('test'); // Should not throw
