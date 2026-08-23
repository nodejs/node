'use strict';

require('../common');
const { DeflateRaw } = require('zlib');
const { Readable } = require('stream');

// Validates that zlib.DeflateRaw can be subclassed with class syntax.

class NotInitialized extends DeflateRaw {
  constructor(options) {
    super(options);
    this.prop = true;
  }
}

const dest = new NotInitialized();

const read = new Readable({
  read() {
    this.push(Buffer.from('a test string'));
    this.push(null);
  }
});

read.pipe(dest);
dest.resume();
