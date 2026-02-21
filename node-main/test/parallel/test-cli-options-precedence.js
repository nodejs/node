'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// The last option on the command line takes precedence:
assert.strictEqual(spawnSync(process.execPath, [
  '--max-http-header-size=1234',
  '--max-http-header-size=5678',
  '-p', 'http.maxHeaderSize',
], {
  encoding: 'utf8'
}).stdout.trim(), '5678');

// The command line takes precedence over NODE_OPTIONS:
assert.strictEqual(spawnSync(process.execPath, [
  '--max-http-header-size=5678',
  '-p', 'http.maxHeaderSize',
], {
  encoding: 'utf8',
  env: { ...process.env, NODE_OPTIONS: '--max-http-header-size=1234' }
}).stdout.trim(), '5678');
