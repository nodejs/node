'use strict';

require('../common');
const { DeflateRaw } = require('zlib');
const { inherits } = require('util');
const { Readable } = require('stream');

// validates that zlib.DeflateRaw can be inherited
// with util.inherits

function NotInitialized(options) {
  DeflateRaw.call(this, options);
  this.prop = true;
}
inherits(NotInitialized, DeflateRaw);

const dest = new NotInitialized();

const read = new Readable({
  read() {
    this.push(Buffer.from('a test string'));
    this.push(null);
  }
});

read.pipe(dest);
dest.resume();
