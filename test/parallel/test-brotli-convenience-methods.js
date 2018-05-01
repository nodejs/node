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
// test convenience methods with and without options supplied

const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');

common.crashOnUnhandledRejection();

// Must be a multiple of 4 characters in total to test all ArrayBufferView
// types.
const expectStr = 'blah'.repeat(8);
const expectBuf = Buffer.from(expectStr);

const opts = {
  wuality: 9,
  chunkSize: 1024,
};

const optsInfo = {
  info: true
};

(async () => {
  for (const [type, expect] of [
    ['string', expectStr],
    ['Buffer', expectBuf],
    ...common.getBufferSources(expectBuf).map((obj) =>
      [obj[Symbol.toStringTag], obj]
    )
  ]) {
    brotli.compress(expect, opts, common.mustCall((err, result) => {
      brotli.decompress(result, opts, common.mustCall((err, result) => {
        assert.strictEqual(result.toString(), expectStr,
                           'Should get original string after compress/' +
                            `decompress ${type} with options.`);
      }));
    }));

    brotli.compress(expect, common.mustCall((err, result) => {
      brotli.decompress(result, common.mustCall((err, result) => {
        assert.strictEqual(result.toString(), expectStr,
                           'Should get original string after compress/' +
                            `decompress ${type} without options.`);
      }));
    }));

    brotli.compress(expect, optsInfo, common.mustCall((err, result) => {
      assert.ok(result.engine instanceof brotli.Compress,
                'Should get engine Compress after compress ' +
                `${type} with info option.`);

      const compressed = result.buffer;
      brotli.decompress(compressed, optsInfo, common.mustCall((err, result) => {
        assert.strictEqual(result.buffer.toString(), expectStr,
                           'Should get original string after compress/' +
                            `decompress ${type} with info option.`);
        assert.ok(result.engine instanceof brotli.Decompress,
                  'Should get engine Decompress after decompress ' +
                  `${type} with info option.`);
      }));
    }));

    {
      const compressed = brotli.compress(expect, opts);
      assert.ok(compressed instanceof Promise,
                'Should get Promise after compress ' +
                 `${type} with option.`);
      const decompressed = brotli.decompress(await compressed, opts);
      assert.ok(decompressed instanceof Promise,
                'Should get Promise after decompress ' +
                `${type} with options.`);
      assert.strictEqual((await decompressed).toString(), expectStr,
                         'Should get original string after compress/' +
                          `decompress ${type} with options.`);
    }

    {
      const compressed = brotli.compress(expect);
      assert.ok(compressed instanceof Promise,
                'Should get Promise after compress ' +
                 `${type} without options.`);
      const decompressed = brotli.decompress(await compressed);
      assert.ok(decompressed instanceof Promise,
                'Should get Promise after decompress ' +
                `${type} without options.`);
      assert.strictEqual((await decompressed).toString(), expectStr,
                         'Should get original string after compress/' +
                          `decompress ${type} without options.`);
    }

    {
      const compressed = brotli.compressSync(expect, opts);
      const decompressed = brotli.decompressSync(compressed, opts);
      assert.strictEqual(decompressed.toString(), expectStr,
                         'Should get original string after compressSync/' +
                          `decompressSync ${type} with options.`);
    }


    {
      const compressed = brotli.compressSync(expect);
      const decompressed = brotli.decompressSync(compressed);
      assert.strictEqual(decompressed.toString(), expectStr,
                         'Should get original string after compressSync/' +
                          `decompressSync ${type} without options.`);
    }


    {
      const compressed = brotli.compressSync(expect, optsInfo);
      assert.ok(compressed.engine instanceof brotli.Compress,
                'Should get engine Compress after compressSync ' +
                 `${type} with info option.`);
      const decompressed = brotli.decompressSync(compressed.buffer,
                                                 optsInfo);
      assert.strictEqual(decompressed.buffer.toString(), expectStr,
                         'Should get original string after compressSync/' +
                          `decompressSync ${type} without options.`);
      assert.ok(decompressed.engine instanceof brotli.Decompress,
                'Should get engine Decompress after decompressSync ' +
                 `${type} with info option.`);
    }
  }
})();
