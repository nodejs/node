'use strict';

const common = require('../common'); // eslint-disable-line no-unused-vars
const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');

{
  const message = 'message';
  process.env.NODE_DEBUG = 'test';
  const { stderr } = spawnSync(process.execPath, [
    path.resolve(__dirname, 'test-common-debuglog-dummy.js'),
    message
  ], { encoding: 'utf8' });

  assert.ok(stderr.startsWith('TEST'));
  assert.ok(stderr.endsWith(`${message}\n`));
}
