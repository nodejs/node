// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { channel } = require('diagnostics_channel');

function test() {
  function subscribe() {
    channel('test-gc').subscribe(function noop() {});
  }

  subscribe();

  setTimeout(common.mustCall(() => {
    global.gc();
    assert.ok(channel('test-gc').hasSubscribers, 'Channel must have subscribers');
  }));
}

test();
