'use strict';

require('../common');
const { spawn } = require('child_process');
const { join } = require('path');
const { fixturesDir } = require('../common/fixtures');

spawn(
  process.execPath,
  [
    join(fixturesDir, 'esm_loader_not_found_cjs_hint_bare.mjs'),
  ],
  {
    cwd: fixturesDir,
    stdio: 'inherit'
  }
);
