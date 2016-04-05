'use strict';
// tests zlib streams with truncated compressed input

require('../common');
const assert = require('assert');
const zlib = require ('zlib');

const inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing eli' +
                    't. Morbi faucibus, purus at gravida dictum, libero arcu ' +
                    'convallis lacus, in commodo libero metus eu nisi. Nullam' +
                    ' commodo, neque nec porta placerat, nisi est fermentum a' +
                    'ugue, vitae gravida tellus sapien sit amet tellus. Aenea' +
                    'n non diam orci. Proin quis elit turpis. Suspendisse non' +
                    ' diam ipsum. Suspendisse nec ullamcorper odio. Vestibulu' +
                    'm arcu mi, sodales non suscipit id, ultrices ut massa. S' +
                    'ed ac sem sit amet arcu malesuada fermentum. Nunc sed. ';

[
  { comp: 'gzip', decomp: 'gunzip', decompSync: 'gunzipSync' },
  { comp: 'gzip', decomp: 'unzip', decompSync: 'unzipSync' },
  { comp: 'deflate', decomp: 'inflate', decompSync: 'inflateSync' },
  { comp: 'deflateRaw', decomp: 'inflateRaw', decompSync: 'inflateRawSync' }
].forEach(function(methods) {
  zlib[methods.comp](inputString, function(err, compressed) {
    assert(!err);
    const truncated = compressed.slice(0, compressed.length / 2);

    // sync sanity
    assert.doesNotThrow(function() {
      const decompressed = zlib[methods.decompSync](compressed);
      assert.equal(decompressed, inputString);
    });

    // async sanity
    zlib[methods.decomp](compressed, function(err, result) {
      assert.ifError(err);
      assert.equal(result, inputString);
    });

    // sync truncated input test
    assert.throws(function() {
      zlib[methods.decompSync](truncated);
    }, /unexpected end of file/);

    // async truncated input test
    zlib[methods.decomp](truncated, function(err, result) {
      assert(/unexpected end of file/.test(err.message));
    });

    const syncFlushOpt = { finishFlush: zlib.Z_SYNC_FLUSH };

    // sync truncated input test, finishFlush = Z_SYNC_FLUSH
    assert.doesNotThrow(function() {
      const result = zlib[methods.decompSync](truncated, syncFlushOpt)
        .toString();
      assert.equal(result, inputString.substr(0, result.length));
    });

    // async truncated input test, finishFlush = Z_SYNC_FLUSH
    zlib[methods.decomp](truncated, syncFlushOpt, function(err, decompressed) {
      assert.ifError(err);

      const result = decompressed.toString();
      assert.equal(result, inputString.substr(0, result.length));
    });
  });
});
