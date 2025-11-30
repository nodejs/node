'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// This test verifies that setupHistory callback errors propagate.
// On main branch (without your fix), this test MUST FAIL.
// After your patch, it MUST PASS.

const child = spawnSync(process.execPath, ['-e', `
  const repl = require('repl');
  const server = repl.start({ prompt: "" });
  server.setupHistory('/tmp/history-test', (err) => {
    throw new Error("INTENTIONAL_CALLBACK_THROW");
  });
`], {
  encoding: 'utf8'
});

// The bug: before your fix, Node *does NOT* propagate the thrown error,
// so stderr does NOT contain the message.
// After your fix, stderr WILL contain INTENTIONAL_CALLBACK_THROW.

assert.match(child.stderr, /INTENTIONAL_CALLBACK_THROW/, 
  'setupHistory callback error did NOT propagate');

