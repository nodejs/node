'use strict';
var common = require('../common');
var assert = require('assert');

// This test verifies that stream.unshift(Buffer(0)) or
// stream.unshift('') does not set state.reading=false.
var Readable = require('stream').Readable;

var r = new Readable();
var nChunks = 10;
var chunk = new Buffer(10);
chunk.fill('x');

r._read = function(n) {
  setTimeout(function() {
    r.push(--nChunks === 0 ? null : chunk);
  });
};

var readAll = false;
var seen = [];
r.on('readable', function() {
  var chunk;
  while (chunk = r.read()) {
    seen.push(chunk.toString());
    // simulate only reading a certain amount of the data,
    // and then putting the rest of the chunk back into the
    // stream, like a parser might do.  We just fill it with
    // 'y' so that it's easy to see which bits were touched,
    // and which were not.
    var putBack = new Buffer(readAll ? 0 : 5);
    putBack.fill('y');
    readAll = !readAll;
    r.unshift(putBack);
  }
});

var expect =
  [ 'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy',
    'xxxxxxxxxx',
    'yyyyy' ];

r.on('end', function() {
  assert.deepEqual(seen, expect);
  console.log('ok');
});
