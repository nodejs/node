'use strict';
const common = require('../common');
const { Readable, Transform } = require('stream');

const src = new Readable({
  read() {
    this.push(Buffer.alloc(20000));
  }
});

const dst = new Transform({
  transform(chunk, output, fn) {
    this.push(null);
    fn();
  }
});

src.pipe(dst);
dst.resume();
dst.once('end', common.mustCall());
