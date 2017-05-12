'use strict';
// tests zlib streams with truncated compressed input

const { tagUnwrap } = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const inputString = tagUnwrap`
  ΩΩLorem ipsum dolor sit amet, consectetur adipiscing elit.
  Morbi faucibus, purus at gravida dictum, libero arcu convallis lacus,
  in commodo libero metus eu nisi. Nullam commodo, neque nec porta placerat,
  nisi est fermentum augue, vitae gravida tellus sapien sit amet tellus.
  Aenean non diam orci. Proin quis elit turpis. Suspendisse non diam ipsum.
  Suspendisse nec ullamcorper odio. Vestibulum arcu mi, sodales non suscipit id,
  ultrices ut massa. Sed ac sem sit amet arcu malesuada fermentum. Nunc sed.
`;

[
  { comp: 'gzip', decomp: 'gunzip', decompSync: 'gunzipSync' },
  { comp: 'gzip', decomp: 'unzip', decompSync: 'unzipSync' },
  { comp: 'deflate', decomp: 'inflate', decompSync: 'inflateSync' },
  { comp: 'deflateRaw', decomp: 'inflateRaw', decompSync: 'inflateRawSync' }
].forEach(function(methods) {
  zlib[methods.comp](inputString, function(err, compressed) {
    assert.ifError(err);
    const truncated = compressed.slice(0, compressed.length / 2);
    const toUTF8 = (buffer) => buffer.toString('utf-8');

    // sync sanity
    assert.doesNotThrow(function() {
      const decompressed = zlib[methods.decompSync](compressed);
      assert.strictEqual(toUTF8(decompressed), inputString);
    });

    // async sanity
    zlib[methods.decomp](compressed, function(err, result) {
      assert.ifError(err);
      assert.strictEqual(toUTF8(result), inputString);
    });

    // sync truncated input test
    assert.throws(function() {
      zlib[methods.decompSync](truncated);
    }, /unexpected end of file/);

    // async truncated input test
    zlib[methods.decomp](truncated, function(err, result) {
      assert(/unexpected end of file/.test(err.message));
    });

    const syncFlushOpt = { finishFlush: zlib.constants.Z_SYNC_FLUSH };

    // sync truncated input test, finishFlush = Z_SYNC_FLUSH
    assert.doesNotThrow(function() {
      const result = toUTF8(zlib[methods.decompSync](truncated, syncFlushOpt));
      assert.strictEqual(result, inputString.substr(0, result.length));
    });

    // async truncated input test, finishFlush = Z_SYNC_FLUSH
    zlib[methods.decomp](truncated, syncFlushOpt, function(err, decompressed) {
      assert.ifError(err);
      const result = toUTF8(decompressed);
      assert.strictEqual(result, inputString.substr(0, result.length));
    });
  });
});
