'use strict';
const common = require('../common');
const assert = require('assert');

const stream = require('stream');
const PassThrough = stream.PassThrough;

const src = new PassThrough({ objectMode: true });
const tx = new PassThrough({ objectMode: true });
const dest = new PassThrough({ objectMode: true });

const expect = [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
const results = [];

dest.on('data', common.mustCall(function(x) {
  results.push(x);
}, expect.length));

src.pipe(tx).pipe(dest);

let i = -1;
const int = setInterval(common.mustCall(function() {
  if (results.length === expect.length) {
    src.end();
    clearInterval(int);
    assert.deepStrictEqual(results, expect);
  } else {
    src.write(i++);
  }
}, expect.length + 1), 1);
