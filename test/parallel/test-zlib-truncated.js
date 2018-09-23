'use strict';
// tests zlib streams with truncated compressed input

const common = require('../common');
const assert = require('assert');
const zlib = require ('zlib');

const inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing el' +
  'it. Morbi faucibus, purus at gravida dictum, libero arcu convallis la' +
  'cus, in commodo libero metus eu nisi. Nullam commodo, neque nec porta' +
  ' placerat, nisi est fermentum augue, vitae gravida tellus sapien sit ' +
  'amet tellus. Aenean non diam orci. Proin quis elit turpis. Suspendiss' +
  'e non diam ipsum. Suspendisse nec ullamcorper odio. Vestibulum arcu m' +
  'i, sodales non suscipit id, ultrices ut massa. Sed ac sem sit amet ar' +
  'cu malesuada fermentum. Nunc sed. ';

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
  });
});
