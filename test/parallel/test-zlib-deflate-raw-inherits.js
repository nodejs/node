'use strict';

require('../common');
const zlib = require('zlib');
const inherits = require('util').inherits;
const { Readable } = require('stream');

// validates that zlib.DeflateRaw can be inherited
// with util.inherits

function NotInitialized(options) {
  zlib.DeflateRaw.call(this, options);
  this.prop = true;
}
inherits(NotInitialized, zlib.DeflateRaw);

const dest = new NotInitialized();

const read = new Readable({
  read() {
    this.push(Buffer.from('a test string'));
    this.push(null);
  }
});

read.pipe(dest);
dest.resume();
