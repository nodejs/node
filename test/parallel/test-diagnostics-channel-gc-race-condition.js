// Flags: --expose-gc
'use strict';

require('../common');
const assert = require('assert');
const { channel } = require('diagnostics_channel');

function test() {
  const testChannel = channel('test-gc');

  setTimeout(() => {
    const testChannel2 = channel('test-gc');

    assert.ok(testChannel === testChannel2, 'Channel instances must be the same');
  });
}

test();

setTimeout(() => {
  global.gc();
  test();
}, 10);
