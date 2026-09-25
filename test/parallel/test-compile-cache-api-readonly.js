'use strict';

// This tests module.enableCompileCache({ directory, readOnly: true }): existing
// entries are read, nothing is written, and a missing directory is not created.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const wrapper = fixtures.path('compile-cache-wrapper-options.js');
const target = path.join(tmpdir.path, 'target.js');
fs.writeFileSync(target, 'module.exports = 1;');
const other = path.join(tmpdir.path, 'other.js');
fs.writeFileSync(other, 'module.exports = 2;');
const directory = path.join(tmpdir.path, 'cache');
const list = () => fs.readdirSync(directory, { recursive: true }).sort();
const run = (options, extraEnv, requires, check) => spawnSyncAndAssert(
  process.execPath,
  [...requires.flatMap((r) => ['-r', r]), target],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
      NODE_TEST_COMPILE_CACHE_OPTIONS: JSON.stringify(options),
      ...extraEnv,
    },
  },
  check);

// Read-only against a directory that does not exist: enabling fails and
// nothing is created.
run({ directory, readOnly: true }, {}, [wrapper], {
  stderr: /read-only cache directory .*\.\.\.not found/,
});
assert(!fs.existsSync(directory));

// A normal run generates the cache for target.js.
run({ directory }, {}, [wrapper], {
  stderr: /writing cache for .*target\.js.*success/,
});
const generated = list();
assert.notStrictEqual(generated.length, 0);

// Read-only against it: target.js's entry is accepted, other.js is compiled
// but not written, and persistence is skipped.
run({ directory, readOnly: true }, {}, [wrapper, other], {
  stderr: common.mustCall((output) => {
    assert.match(output, /cache for .*target\.js was accepted/);
    assert.match(output, /read-only, skipping persistence/);
    assert.doesNotMatch(output, /writing cache for/);
    return true;
  }),
});
assert.deepStrictEqual(list(), generated);

// The environment variable form.
run({ directory }, { NODE_COMPILE_CACHE_READONLY: '1' }, [wrapper, other], {
  stderr: /read-only, skipping persistence/,
});
assert.deepStrictEqual(list(), generated);
