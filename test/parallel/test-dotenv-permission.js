'use strict';

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const path = require('node:path');

{
  // Check if `experimental-permission` flag applies to `--env-file` flag as well.
  const response = spawnSync(
    process.execPath,
    ['--env-file', path.join(__dirname, '../fixtures/dotenv/valid.env'), '--experimental-permission'],
  );

  const stderr = response.stderr.toString();
  assert(stderr.includes('ERR_ACCESS_DENIED'));
  assert(stderr.includes('FileSystemRead'));
}
