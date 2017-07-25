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
const zlib = require('zlib');
const path = require('path');
const fs = require('fs');
const util = require('util');
const stream = require('stream');

let zlibPairs = [
  [zlib.Deflate, zlib.Inflate],
  [zlib.Gzip, zlib.Gunzip],
  [zlib.Deflate, zlib.Unzip],
  [zlib.Gzip, zlib.Unzip],
  [zlib.DeflateRaw, zlib.InflateRaw]
];

// how fast to trickle through the slowstream
let trickle = [128, 1024, 1024 * 1024];

// tunable options for zlib classes.

// several different chunk sizes
let chunkSize = [128, 1024, 1024 * 16, 1024 * 1024];

// this is every possible value.
let level = [-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
let windowBits = [8, 9, 10, 11, 12, 13, 14, 15];
let memLevel = [1, 2, 3, 4, 5, 6, 7, 8, 9];
let strategy = [0, 1, 2, 3, 4];

// it's nice in theory to test every combination, but it
// takes WAY too long.  Maybe a pummel test could do this?
if (!process.env.PUMMEL) {
  trickle = [1024];
  chunkSize = [1024 * 16];
  level = [6];
  memLevel = [8];
  windowBits = [15];
  strategy = [0];
}

let testFiles = ['person.jpg', 'elipses.txt', 'empty.txt'];

if (process.env.FAST) {
  zlibPairs = [[zlib.Gzip, zlib.Unzip]];
  testFiles = ['person.jpg'];
}

const tests = {};
testFiles.forEach(function(file) {
  tests[file] = fs.readFileSync(path.resolve(common.fixturesDir, file));
});


// stream that saves everything
function BufferStream() {
  this.chunks = [];
  this.length = 0;
  this.writable = true;
  this.readable = true;
}

util.inherits(BufferStream, stream.Stream);

BufferStream.prototype.write = function(c) {
  this.chunks.push(c);
  this.length += c.length;
  return true;
};

BufferStream.prototype.end = function(c) {
  if (c) this.write(c);
  // flatten
  const buf = Buffer.allocUnsafe(this.length);
  let i = 0;
  this.chunks.forEach(function(c) {
    c.copy(buf, i);
    i += c.length;
  });
  this.emit('data', buf);
  this.emit('end');
  return true;
};


function SlowStream(trickle) {
  this.trickle = trickle;
  this.offset = 0;
  this.readable = this.writable = true;
}

util.inherits(SlowStream, stream.Stream);

SlowStream.prototype.write = function() {
  throw new Error('not implemented, just call ss.end(chunk)');
};

SlowStream.prototype.pause = function() {
  this.paused = true;
  this.emit('pause');
};

SlowStream.prototype.resume = function() {
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
};

SlowStream.prototype.end = function(chunk) {
  // walk over the chunk in blocks.
  this.chunk = chunk;
  this.length = chunk.length;
  this.resume();
  return this.ended;
};


// for each of the files, make sure that compressing and
// decompressing results in the same data, for every combination
// of the options set above.
let failures = 0;
let total = 0;
let done = 0;

Object.keys(tests).forEach(function(file) {
  const test = tests[file];
  chunkSize.forEach(function(chunkSize) {
    trickle.forEach(function(trickle) {
      windowBits.forEach(function(windowBits) {
        level.forEach(function(level) {
          memLevel.forEach(function(memLevel) {
            strategy.forEach(function(strategy) {
              zlibPairs.forEach(function(pair) {
                const Def = pair[0];
                const Inf = pair[1];
                const opts = { level: level,
                               windowBits: windowBits,
                               memLevel: memLevel,
                               strategy: strategy };

                total++;

                const def = new Def(opts);
                const inf = new Inf(opts);
                const ss = new SlowStream(trickle);
                const buf = new BufferStream();

                // verify that the same exact buffer comes out the other end.
                buf.on('data', function(c) {
                  const msg = `${file} ${chunkSize} ${
                    JSON.stringify(opts)} ${Def.name} -> ${Inf.name}`;
                  let ok = true;
                  const testNum = ++done;
                  let i;
                  for (i = 0; i < Math.max(c.length, test.length); i++) {
                    if (c[i] !== test[i]) {
                      ok = false;
                      failures++;
                      break;
                    }
                  }
                  if (ok) {
                    console.log(`ok ${testNum} ${msg}`);
                  } else {
                    console.log(`not ok ${testNum} msg`);
                    console.log('  ...');
                    console.log(`  testfile: ${file}`);
                    console.log(`  type: ${Def.name} -> ${Inf.name}`);
                    console.log(`  position: ${i}`);
                    console.log(`  options: ${JSON.stringify(opts)}`);
                    console.log(`  expect: ${test[i]}`);
                    console.log(`  actual: ${c[i]}`);
                    console.log(`  chunkSize: ${chunkSize}`);
                    console.log('  ---');
                  }
                });

                // the magic happens here.
                ss.pipe(def).pipe(inf).pipe(buf);
                ss.end(test);
              });
            });
          });
        });
      });
    });
  });
});

process.on('exit', function(code) {
  console.log(`1..${done}`);
  assert.strictEqual(done, total, `${total - done} tests left unfinished`);
  assert.strictEqual(failures, 0, 'some test failures');
});
