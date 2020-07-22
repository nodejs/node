'use strict';
// Flags: --expose-internals
require('../common');
const { getOptionValue } = require('internal/options');
const assert = require('assert');
const cp = require('child_process');

const expected_redirect_value = 'foó';

if (process.argv.length === 2) {
  const NODE_OPTIONS = `--redirect-warnings=${expected_redirect_value}`;
  const result = cp.spawnSync(process.argv0,
                              ['--expose-internals', __filename, 'test'],
                              {
                                env: {
                                  NODE_OPTIONS
                                }
                              });
  assert.strictEqual(result.status, 0);
  process.exit(0);
} else {
  const redirect_value = getOptionValue('--redirect-warnings');
  assert.strictEqual(redirect_value, expected_redirect_value);
}
