'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');
const PassThrough = stream.PassThrough;

const src = new PassThrough({ objectMode: true });
const tx = new PassThrough({ objectMode: true });
const dest = new PassThrough({ objectMode: true });

const expect = [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
const results = [];
process.on('exit', function() {
  assert.deepStrictEqual(results, expect);
  console.log('ok');
});

dest.on('data', function(x) {
  results.push(x);
});

src.pipe(tx).pipe(dest);

let i = -1;
const int = setInterval(function() {
  if (i > 10) {
    src.end();
    clearInterval(int);
  } else {
    src.write(i++);
  }
}, 1);
