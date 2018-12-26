'use strict';

// This test confirms that `undefined`, `null`, and `[]` can be used
// as a placeholder for the second argument (`args`) of `spawnSync()`.
// Previously, there was a bug where using `undefined` for the second argument
// caused the third argument (`options`) to be ignored.
// See https://github.com/nodejs/node/issues/24912.

const assert = require('assert');
const { spawnSync } = require('child_process');

const common = require('../common');
const tmpdir = require('../common/tmpdir');

const command = common.isWindows ? 'cd' : 'pwd';
const options = { cwd: tmpdir.path };

tmpdir.refresh();

if (common.isWindows) {
  // This test is not the case for Windows based systems
  // unless the `shell` options equals to `true`

  options.shell = true;
}

const testCases = [
  undefined,
  null,
  [],
];

const expectedResult = tmpdir.path.trim().toLowerCase();

const results = testCases.map((testCase) => {
  const { stdout, stderr, error } = spawnSync(
    command,
    testCase,
    options
  );

  assert.ifError(error);
  assert.deepStrictEqual(stderr, Buffer.alloc(0));

  return stdout.toString().trim().toLowerCase();
});

assert.deepStrictEqual([...new Set(results)], [expectedResult]);
