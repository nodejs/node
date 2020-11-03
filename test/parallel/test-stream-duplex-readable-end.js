'use strict';
// https://github.com/nodejs/node/issues/35926
require('../common');
const assert = require('assert');
const stream = require('stream');

let loops = 5;

const src = new stream.Readable({
  read() {
    if (loops--)
      this.push(Buffer.alloc(20000));
  }
});

const dst = new stream.Transform({
  transform(chunk, output, fn) {
    this.push(null);
    fn();
  }
});

src.pipe(dst);

function parser_end() {
  assert.ok(loops > 0);
  dst.removeAllListeners();
}

dst.on('data', () => { });
dst.on('end', parser_end);
dst.on('error', parser_end);
