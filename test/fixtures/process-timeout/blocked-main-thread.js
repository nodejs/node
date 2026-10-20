'use strict';

// Blocks the main thread in a synchronous native call, which cannot be
// interrupted. The first argument is a path at which a file is created once the
// main thread is blocked.
const { execFileSync } = require('child_process');
const path = require('path');

execFileSync(
  process.execPath,
  [path.join(__dirname, 'wait-for-parent.js'), String(process.pid), process.argv[2]],
  { stdio: 'ignore' },
);
