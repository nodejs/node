// Flags: --expose-gc
'use strict';

require('../common');
const assert = require('assert');
const { channel } = require('diagnostics_channel');

function test() {
  function subscribe() {
    channel('test-gc').subscribe(function noop() {});
  }

  subscribe();

  setTimeout(() => {
    global.gc();
    assert.ok(channel('test-gc').hasSubscribers, 'Channel must have subscribers');
  });
}

test();
