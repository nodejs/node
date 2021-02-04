'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
if (process.argv[2] === 'single-dep-skip') {
  new Buffer(10);
} else if (process.argv[2] === 'multi-dep-skip') {
  new Buffer(10);
  tmpdir.refresh();
  const filePath = path.join(tmpdir.path, 'rmdir-recursive.txt');
  fs.rmdir(filePath, { recursive: true }, () => {});
} else if (process.argv[2] === 'single-exp-skip') {
  const vm = require('vm');
  vm.measureMemory();
} else if (process.argv[2] === 'multi-exp-skip') {
  const vm = require('vm');
  vm.measureMemory();
  new AbortController();
} else {
  const child_single_dep_skip = cp.spawnSync(process.execPath,
                                             ['--suppress-warnings=DEP0005',
                                              __filename, 'single-dep-skip']);
  assert.strictEqual(child_single_dep_skip.stdout.toString(), '');
  assert.strictEqual(child_single_dep_skip.stderr.toString(), '');
  const child_multi_dep_skip = cp.spawnSync(process.execPath,
                                            ['--suppress-warnings=' +
                                             'DEP0005,DEP0147',
                                             __filename,
                                             'multi-dep-skip']);
  assert.strictEqual(child_multi_dep_skip.stdout.toString(), '');
  assert.strictEqual(child_multi_dep_skip.stderr.toString(), '');
  const child_single_exp_skip = cp.spawnSync(process.execPath,
                                             ['--suppress-warnings=EXP0004',
                                              __filename, 'single-exp-skip']);
  assert.strictEqual(child_single_exp_skip.stdout.toString(), '');
  assert.strictEqual(child_single_exp_skip.stderr.toString(), '');
  const child_multi_exp_skip = cp.spawnSync(process.execPath,
                                            ['--suppress-warnings=' +
                                             'EXP0004,EXP0001',
                                             __filename,
                                             'multi-exp-skip']);
  assert.strictEqual(child_multi_exp_skip.stdout.toString(), '');
  assert.strictEqual(child_multi_exp_skip.stderr.toString(), '');
}
