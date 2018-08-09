'use strict';

const common = require('../common');
const assert = require('assert');

const message = 'message';
const debugEnv = (process.env.NODE_DEBUG || '').split(',');

if (!debugEnv.includes('test')) {
  process.env.NODE_DEBUG = 'test,' + (process.env.NODE_DEBUG || '');
}

const listener = common.mustCallAtLeast((data) => {
  assert.ok(data.startsWith('TEST'));
  assert.ok(data.endsWith(message + '\n'));
}, 1);

common.hijackStderr(listener);
common.debuglog(message);
assert.ok(process.stderr.writeTimes, 'not called!');
common.restoreStderr();
