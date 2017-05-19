'use strict';

require('../common');
const assert = require('assert');
const stream = require('stream');

var queue = [];
for (var uncork = 0; uncork < 2; uncork++) {
  for (var multi = 0; multi < 2; multi++) {
    queue.push([!!uncork, !!multi]);
  }
}

run();

function run() {
  var t = queue.pop();
  if (t)
    test(t[0], t[1], run);
}

function test(uncork, multi, next) {
  var counter = 0;
  var expectCount = 0;
  function cnt(msg) {
    expectCount++;
    var expect = expectCount;
    return function(er) {
      if (er)
        throw er;
      counter++;
      assert.equal(counter, expect);
    };
  }

  var w = new stream.Writable({ objectMode: true });
  w._write = function(chunk, e, cb) {
    assert(false, 'Should not call _write');
  };

  var expectChunks = [
    { text: 'hello, ' },
    'world',
    [33],
    { prop: { name: 'qwerty'} }
  ];

  var actualChunks;
  w._writev = function(chunks, cb) {
    actualChunks = chunks;
    cb();
  };

  w.cork();
  w.write({ text: 'hello, ' }, cnt('simple object'));
  w.write('world', cnt('world'));

  if (multi)
    w.cork();

  w.write([33], cnt('array'));

  if (multi)
    w.uncork();

  w.write({ prop: { name: 'qwerty'} }, cnt('complex object'));

  if (uncork)
    w.uncork();

  w.end(cnt('end'));

  w.on('finish', function() {
    // make sure finish comes after all the write cb
    cnt('finish')();
    assert.deepEqual(expectChunks, actualChunks);
    next();
  });
}
