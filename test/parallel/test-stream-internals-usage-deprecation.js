// Flags: --no-warnings
// The flag suppresses stderr output but the warning event will still emit
'use strict';

const common = require('../common');
const assert = require('assert');

const mods = [
  '_stream_readable',
  '_stream_writable',
  '_stream_duplex',
  '_stream_transform',
  '_stream_passthrough'
];

// we cannot use a forEach, the warning is emitted asynchronously
function test() {
  const mod = mods.shift();

  if (!mod) {
    return;
  }

  process.once('warning', common.mustCall((warning) => {
    assert.ok(warning instanceof Error);
    assert.strictEqual(warning.message,
                       `The ${mod}.js module is deprecated. Please use stream`);
    assert.strictEqual(warning.code, 'DEP00XX');
    test();
  }));

  require(mod);
}

test();
