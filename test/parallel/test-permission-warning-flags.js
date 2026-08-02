'use strict';

require('../common');
const { spawnSync } = require('child_process');
const assert = require('assert');

const warnFlags = [
  '--allow-addons',
  '--allow-child-process',
  '--allow-inspector',
  '--allow-wasi',
  '--allow-worker',
];

if (process.config.variables.node_use_ffi) {
  warnFlags.push('--allow-ffi');
}

for (const flag of warnFlags) {
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission', flag, '-e',
      'setTimeout(() => {}, 1)',
    ]
  );

  assert.match(stderr.toString(), new RegExp(`\\[PERM\\d{4}\\] SecurityWarning: The flag ${RegExp.escape(flag)} must be used with extreme caution`));
  assert.strictEqual(status, 0);
}

const { status, stderr } = spawnSync(
  process.execPath,
  [
    '--permission', '--allow-child-process', '--allow-wasi', '--disable-warning=PERM0002', '-e',
    'setTimeout(() => {}, 1)',
  ]
);

// Disabled warning does not appear
assert.doesNotMatch(stderr.toString(), new RegExp(`The flag --allow-child-process must be used with extreme caution`));
// But non-disabled warnings still appear
assert.match(stderr.toString(), new RegExp(`The flag --allow-wasi must be used with extreme caution`));
assert.strictEqual(status, 0);
