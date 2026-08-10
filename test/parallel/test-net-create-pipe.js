'use strict';
const assert = require('node:assert');
const { testCreatePipe } = require('../common/net-create-pipe');

testCreatePipe('createPipe returns directional OS-backed streams',
  (readable, writable) => {
    assert.strictEqual(readable.readable, true);
    assert.strictEqual(readable.writable, false);
    assert.strictEqual(writable.readable, false);
    assert.strictEqual(writable.writable, true);

    readable.resume();
    writable.end();
  });

testCreatePipe('readable starts paused so parent does not pre-consume bytes',
  (readable, writable) => {
    assert.strictEqual(readable.readableFlowing, false);

    readable.resume();
    writable.end();
  });
