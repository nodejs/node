'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// Check that conflicting TLS protocol versions are not allowed

const assert = require('assert');
const child_process = require('child_process');

const args = ['--tls-min-v1.3', '--tls-max-v1.2', '-p', 'process.version'];
child_process.execFile(process.argv[0], args, (err) => {
  assert(err);
  assert.match(err.message, /not both/);
});
