'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

const handle = v8.startHeapProfile();
try {
  v8.startHeapProfile();
} catch (err) {
  assert.strictEqual(err.code, 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED');
}
const profile = handle.stop();
assert.ok(typeof profile === 'string');
assert.ok(profile.length > 0);
