'use strict';
require('../common');
var stream = require('stream');
var assert = require('assert');

class Writable extends stream.Stream {
  constructor() {
    super();
    this.writable = true;
  }
}

class Readable extends stream.Stream {
  constructor() {
    super();
    this.readable = true;
  }
}
var passed = false;

var w = new Writable();
w.on('pipe', function(src) {
  passed = true;
});

var r = new Readable();
r.pipe(w);

assert.ok(passed);
