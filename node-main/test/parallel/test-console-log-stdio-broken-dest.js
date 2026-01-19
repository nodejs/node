'use strict';

const common = require('../common');
const { Writable } = require('stream');
const { Console } = require('console');
const { EventEmitter } = require('events');

const stream = new Writable({
  write(chunk, enc, cb) {
    cb();
  },
  writev(chunks, cb) {
    setTimeout(cb, 10, new Error('kaboom'));
  }
});
const myConsole = new Console(stream, stream);

process.on('warning', common.mustNotCall());

stream.cork();
for (let i = 0; i < EventEmitter.defaultMaxListeners + 1; i++) {
  myConsole.log('a message');
}
stream.uncork();
