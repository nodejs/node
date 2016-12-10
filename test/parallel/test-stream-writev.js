'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');

var queue = [];
for (var decode = 0; decode < 2; decode++) {
  for (var uncork = 0; uncork < 2; uncork++) {
    for (var multi = 0; multi < 2; multi++) {
      queue.push([!!decode, !!uncork, !!multi]);
    }
  }
}

run();

function run() {
  var t = queue.pop();
  if (t)
    test(t[0], t[1], t[2], run);
  else
    console.log('ok');
}

function test(decode, uncork, multi, next) {
  console.log('# decode=%j uncork=%j multi=%j', decode, uncork, multi);
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

  var w = new stream.Writable({ decodeStrings: decode });
  w._write = function(chunk, e, cb) {
    assert(false, 'Should not call _write');
  };

  var expectChunks = decode ? [
    { encoding: 'buffer',
      chunk: [104, 101, 108, 108, 111, 44, 32] },
    { encoding: 'buffer',
      chunk: [119, 111, 114, 108, 100] },
    { encoding: 'buffer',
      chunk: [33] },
    { encoding: 'buffer',
      chunk: [10, 97, 110, 100, 32, 116, 104, 101, 110, 46, 46, 46] },
    { encoding: 'buffer',
      chunk: [250, 206, 190, 167, 222, 173, 190, 239, 222, 202, 251, 173]}
  ] : [
    { encoding: 'ascii', chunk: 'hello, ' },
    { encoding: 'utf8', chunk: 'world' },
    { encoding: 'buffer', chunk: [33] },
    { encoding: 'latin1', chunk: '\nand then...' },
    { encoding: 'hex', chunk: 'facebea7deadbeefdecafbad' }
  ];

  var actualChunks;
  w._writev = function(chunks, cb) {
    actualChunks = chunks.map(function(chunk) {
      return {
        encoding: chunk.encoding,
        chunk: Buffer.isBuffer(chunk.chunk) ?
            Array.prototype.slice.call(chunk.chunk) : chunk.chunk
      };
    });
    cb();
  };

  w.cork();
  w.write('hello, ', 'ascii', cnt('hello'));
  w.write('world', 'utf8', cnt('world'));

  if (multi)
    w.cork();

  w.write(Buffer.from('!'), 'buffer', cnt('!'));
  w.write('\nand then...', 'latin1', cnt('and then'));

  if (multi)
    w.uncork();

  w.write('facebea7deadbeefdecafbad', 'hex', cnt('hex'));

  if (uncork)
    w.uncork();

  w.end(cnt('end'));

  w.on('finish', function() {
    // make sure finish comes after all the write cb
    cnt('finish')();
    assert.deepStrictEqual(expectChunks, actualChunks);
    next();
  });
}
