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

// Flags: --expose-brotli
'use strict';
const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');
const stream = require('stream');
const fixtures = require('../common/fixtures');

// how fast to trickle through the slowstream
let trickle = [128, 1024, 1024 * 1024];

// tunable options for brotli classes.

// several different chunk sizes
let chunkSize = [128, 1024, 1024 * 16, 1024 * 1024];

// this is every possible value.
let quality = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
let lgwin = [10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
             21, 22, 23, 24, 25, 26, 27, 28, 29, 30];

// it's nice in theory to test every combination, but it
// takes WAY too long.  Maybe a pummel test could do this?
if (!process.env.PUMMEL) {
  trickle = [1024];
  chunkSize = [1024 * 16];
  quality = [4];
  lgwin = [22];
}

let testFiles = ['person.jpg', 'elipses.txt', 'empty.txt'];

if (process.env.FAST) {
  testFiles = ['person.jpg'];
}

const tests = {};
testFiles.forEach(common.mustCall((file) => {
  tests[file] = fixtures.readSync(file);
}, testFiles.length));


// stream that saves everything
class BufferStream extends stream.Stream {
  constructor() {
    super();
    this.chunks = [];
    this.length = 0;
    this.writable = true;
    this.readable = true;
  }

  write(c) {
    this.chunks.push(c);
    this.length += c.length;
    return true;
  }

  end(c) {
    if (c) this.write(c);
    // flatten
    const buf = Buffer.allocUnsafe(this.length);
    let i = 0;
    this.chunks.forEach((c) => {
      c.copy(buf, i);
      i += c.length;
    });
    this.emit('data', buf);
    this.emit('end');
    return true;
  }
}

class SlowStream extends stream.Stream {
  constructor(trickle) {
    super();
    this.trickle = trickle;
    this.offset = 0;
    this.readable = this.writable = true;
  }

  write() {
    throw new Error('not implemented, just call ss.end(chunk)');
  }

  pause() {
    this.paused = true;
    this.emit('pause');
  }

  resume() {
    const emit = () => {
      if (this.paused) return;
      if (this.offset >= this.length) {
        this.ended = true;
        return this.emit('end');
      }
      const end = Math.min(this.offset + this.trickle, this.length);
      const c = this.chunk.slice(this.offset, end);
      this.offset += c.length;
      this.emit('data', c);
      process.nextTick(emit);
    };

    if (this.ended) return;
    this.emit('resume');
    if (!this.chunk) return;
    this.paused = false;
    emit();
  }

  end(chunk) {
    // walk over the chunk in blocks.
    this.chunk = chunk;
    this.length = chunk.length;
    this.resume();
    return this.ended;
  }
}

// for each of the files, make sure that compressing and
// decompressing results in the same data, for every combination
// of the options set above.

const testKeys = Object.keys(tests);
testKeys.forEach(common.mustCall((file) => {
  const test = tests[file];
  chunkSize.forEach(common.mustCall((chunkSize) => {
    trickle.forEach(common.mustCall((trickle) => {
      lgwin.forEach(common.mustCall((lgwin) => {
        quality.forEach(common.mustCall((quality) => {
          const opts = { lgwin, quality, largeWindow: lgwin > 24 };

          const enc = new brotli.Compress(opts);
          const dec = new brotli.Decompress(opts);
          const ss = new SlowStream(trickle);
          const buf = new BufferStream();

          // verify that the same exact buffer comes out the other end.
          buf.on('data', common.mustCall((c) => {
            const msg = `${file} ${chunkSize} ${
              JSON.stringify(opts)} Compress -> Decompress`;
            let i;
            for (i = 0; i < Math.max(c.length, test.length); i++) {
              if (c[i] !== test[i]) {
                assert.fail(msg);
                break;
              }
            }
          }));

          // the magic happens here.
          ss.pipe(enc).pipe(dec).pipe(buf);
          ss.end(test);
        }, quality.length));
      }, lgwin.length));
    }, trickle.length));
  }, chunkSize.length));
}, testKeys.length));
