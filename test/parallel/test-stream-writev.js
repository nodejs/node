// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

const stream = require('stream');

const queue = [];
for (let decode = 0; decode < 2; decode++) {
  for (let uncork = 0; uncork < 2; uncork++) {
    for (let multi = 0; multi < 2; multi++) {
      queue.push([!!decode, !!uncork, !!multi]);
    }
  }
}

run();

function run() {
  const t = queue.pop();
  if (t)
    test(t[0], t[1], t[2], run);
  else
    console.log('ok');
}

function test(decode, uncork, multi, next) {
  console.log(`# decode=${decode} uncork=${uncork} multi=${multi}`);
  let counter = 0;
  let expectCount = 0;
  function cnt(msg) {
    expectCount++;
    const expect = expectCount;
    return function(er) {
      assert.ifError(er);
      counter++;
      assert.strictEqual(counter, expect);
    };
  }

  const w = new stream.Writable({ decodeStrings: decode });
  w._write = common.mustNotCall('Should not call _write');

  const expectChunks = decode ? [
    { encoding: 'buffer',
      chunk: [104, 101, 108, 108, 111, 44, 32] },
    { encoding: 'buffer',
      chunk: [119, 111, 114, 108, 100] },
    { encoding: 'buffer',
      chunk: [33] },
    { encoding: 'buffer',
      chunk: [10, 97, 110, 100, 32, 116, 104, 101, 110, 46, 46, 46] },
    { encoding: 'buffer',
      chunk: [250, 206, 190, 167, 222, 173, 190, 239, 222, 202, 251, 173] }
  ] : [
    { encoding: 'ascii', chunk: 'hello, ' },
    { encoding: 'utf8', chunk: 'world' },
    { encoding: 'buffer', chunk: [33] },
    { encoding: 'latin1', chunk: '\nand then...' },
    { encoding: 'hex', chunk: 'facebea7deadbeefdecafbad' }
  ];

  let actualChunks;
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
    assert.deepStrictEqual(actualChunks, expectChunks);
    next();
  });
}
