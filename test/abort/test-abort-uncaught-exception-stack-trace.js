'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

function thrower() {
  throw new Error();
}

try {
  thrower();
} catch (err) {
  if (process.argv.includes('child')) {
    throw err;
  }

  const args = ['--abort-on-uncaught-exception', __filename, 'child'];
  const child = spawnSync(process.execPath, args);
  const stackTrace = child.stderr.toString();
  assert(stackTrace.includes(err.stack), 'Error.stack not present in stderr');
}
