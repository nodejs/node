'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');
var PassThrough = stream.PassThrough;

var src = new PassThrough({ objectMode: true });
var tx = new PassThrough({ objectMode: true });
var dest = new PassThrough({ objectMode: true });

var expect = [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
var results = [];
process.on('exit', function() {
  assert.deepStrictEqual(results, expect);
  console.log('ok');
});

dest.on('data', function(x) {
  results.push(x);
});

src.pipe(tx).pipe(dest);

var i = -1;
setImmediate(function int() {
  if (i > 10) {
    src.end();
  } else {
    src.write(i++);
    setImmediate(int);
  }
});
