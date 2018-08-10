'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');

const message = 'message';

{
  process.env.NODE_DEBUG = 'test';
  const { stderr } = spawnSync(process.execPath, [
    path.resolve(__dirname, '../fixtures/common-debuglog.js'),
    message
  ], { encoding: 'utf8' });

  assert.ok(stderr.toString().startsWith('TEST'));
  assert.ok(stderr.toString().trim().endsWith(message));
}

{
  delete process.env.NODE_DEBUG;
  const { stderr } = spawnSync(process.execPath, [
    path.resolve(__dirname, '../fixtures/common-debuglog.js'),
    message
  ], { encoding: 'utf8' });

  assert.strictEqual(stderr.toString().trim(), '');
}

{
  process.env.NODE_DEBUG = 'fs';
  const { stderr } = spawnSync(process.execPath, [
    path.resolve(__dirname, '../fixtures/common-debuglog.js'),
    message
  ], { encoding: 'utf8' });

  assert.strictEqual(stderr.toString().trim(), '');
}
