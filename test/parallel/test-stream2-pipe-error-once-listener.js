'use strict';
require('../common');

const util = require('util');
const stream = require('stream');


const Read = function() {
  stream.Readable.call(this);
};
util.inherits(Read, stream.Readable);

Read.prototype._read = function(size) {
  this.push('x');
  this.push(null);
};


const Write = function() {
  stream.Writable.call(this);
};
util.inherits(Write, stream.Writable);

Write.prototype._write = function(buffer, encoding, cb) {
  this.emit('error', new Error('boom'));
  this.emit('alldone');
};

const read = new Read();
const write = new Write();

write.once('error', function(err) {});
write.once('alldone', function(err) {
  console.log('ok');
});

process.on('exit', function(c) {
  console.error('error thrown even with listener');
});

read.pipe(write);
