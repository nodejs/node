'use strict';

// Test that extensionless files containing ESM syntax are not silently
// swallowed when the nearest package.json has "type": "commonjs".
// Regression test for https://github.com/nodejs/node/issues/61104

const common = require('../common');
const assert = require('assert');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = path.join(tmpdir.path, 'esm-extensionless');
fs.mkdirSync(dir, { recursive: true });

// Create package.json with "type": "commonjs"
fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({
  type: 'commonjs',
}));

// Create an extensionless script with ESM syntax (simulating a CLI tool with a shebang)
const script = path.join(dir, 'script');
fs.writeFileSync(script, `#!/usr/bin/env node
process.exitCode = 42;
export {};
`);
fs.chmodSync(script, 0o755);

// The script should either run as ESM (exit code 42) or throw an error.
// It must NOT silently exit with code 0.
try {
  const result = execFileSync(process.execPath, [script], {
    encoding: 'utf8',
    stdio: ['pipe', 'pipe', 'pipe'],
  });
  // If we reach here, the script ran without error.
  // The exit code should be 42 (set by process.exitCode in the ESM script).
  assert.fail('Expected the script to either exit with code 42 or throw an error, but it exited with code 0');
} catch (err) {
  // execFileSync throws if exit code is non-zero, which is expected.
  // Either exit code 42 (ESM ran correctly) or an error was thrown (also acceptable).
  if (err.status !== null) {
    // The script ran but exited non-zero — ESM was properly detected and executed.
    assert.strictEqual(err.status, 42,
      `Expected exit code 42 from ESM script, got ${err.status}. stderr: ${err.stderr}`);
  }
  // If there's a stderr message about ESM/CommonJS mismatch, that's also acceptable
  // as long as it's not a silent success.
}
