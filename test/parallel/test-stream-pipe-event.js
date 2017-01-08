'use strict';
require('../common');
const stream = require('stream');
const assert = require('assert');
const util = require('util');

function Writable() {
  this.writable = true;
  stream.Stream.call(this);
}
util.inherits(Writable, stream.Stream);

function Readable() {
  this.readable = true;
  stream.Stream.call(this);
}
util.inherits(Readable, stream.Stream);

let passed = false;

const w = new Writable();
w.on('pipe', function(src) {
  passed = true;
});

const r = new Readable();
r.pipe(w);

assert.ok(passed);
