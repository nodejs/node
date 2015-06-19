'use strict';
var common = require('../common');
var assert = require('assert');

var stream = require('stream');
// tiny node-tap lookalike.
var tests = [];
var count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  var next = tests.shift();
  if (!next)
    return console.error('ok');

  var name = next[0];
  var fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepEqual,
    equal: assert.equal,
    end: function() {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on('exit', function() {
  assert.equal(count, 0);
});

process.nextTick(run);
test('all sync', function (t) {
  var counter = 0;
  var expectCount = 0;
  function cnt(msg) {
    expectCount++;
    var expect = expectCount;
    var called = false;
    return function(er) {
      if (er)
        throw er;
      called = true;
      counter++;
      t.equal(counter, expect);
    };
  }
  var p = new stream.PassThrough();
  var w = new stream.Writable();
  w._write = function(chunk, e, cb) {
    assert(false, 'Should not call _write');
  };
  p.pipe(w, {
    cork: true
  });
  var expectChunks =
      [
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
  p.write('hello, ', 'ascii', cnt('hello'));
  p.write('world', 'utf8', cnt('world'));

  p.write(new Buffer('!'), 'buffer', cnt('!'));

  p.write('\nand then...', 'binary', cnt('and then'));
  p.write('facebea7deadbeefdecafbad', 'hex', cnt('hex'));

  p.end(cnt('end'));


  w.on('finish', function() {
    // make sure finish comes after all the write cb
    cnt('finish')();
    t.same(expectChunks, actualChunks);
    t.end();
  });
});
test('2 groups', function (t) {
  var counter = 0;
  var expectCount = 0;
  function cnt(msg) {
    expectCount++;
    var expect = expectCount;
    var called = false;
    return function(er) {
      if (er)
        throw er;
      called = true;
      counter++;
      t.equal(counter, expect);
    };
  }
  var p = new stream.PassThrough();
  var w = new stream.Writable();
  w._write = function(chunk, e, cb) {
    assert(false, 'Should not call _write');
  };
  p.pipe(w, {
    cork: true
  });
  var expectChunks = [
      [
        { encoding: 'buffer',
          chunk: [104, 101, 108, 108, 111, 44, 32] },
        { encoding: 'buffer',
          chunk: [119, 111, 114, 108, 100] }
      ],[
        { encoding: 'buffer',
          chunk: [33] },
        { encoding: 'buffer',
          chunk: [10, 97, 110, 100, 32, 116, 104, 101, 110, 46, 46, 46] },
        { encoding: 'buffer',
          chunk: [250, 206, 190, 167, 222, 173, 190, 239, 222, 202, 251, 173]}
      ]];

  var actualChunks = [];
  var called = 0;
  w._writev = function(chunks, cb) {
    actualChunks.push(chunks.map(function(chunk) {
      return {
        encoding: chunk.encoding,
        chunk: Buffer.isBuffer(chunk.chunk) ?
            Array.prototype.slice.call(chunk.chunk) : chunk.chunk
      };
    }));
    cb();
  };
  p.write('hello, ', 'ascii', cnt('hello'));
  p.write('world', 'utf8', cnt('world'));
  process.nextTick(function () {
    p.write(new Buffer('!'), 'buffer', cnt('!'));

    p.write('\nand then...', 'binary', cnt('and then'));
    p.write('facebea7deadbeefdecafbad', 'hex', cnt('hex'));

    p.end(cnt('end'));
  });

  w.on('finish', function() {
    // make sure finish comes after all the write cb
    cnt('finish')();
    console.log('expected');
    console.log(expectChunks);
    console.log('actual');
    console.log(actualChunks);
    t.same(expectChunks, actualChunks);
    t.end();
  });
});
