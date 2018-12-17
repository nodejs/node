'use strict';

// This test confirms that `undefined`, `null`, and `[]`
// can be used as a placeholder for the second argument (`args`) of `spawn()`.
// Previously, there was a bug where using `undefined` for the second argument
// caused the third argument (`options`) to be ignored.
// See https://github.com/nodejs/node/issues/24912.

const assert = require('assert');
const { spawn } = require('child_process');

const common = require('../common');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const command = common.isWindows ? 'cd' : 'pwd';
const options = { cwd: tmpdir.path };

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

(async () => {
  const results = await Promise.all(
    testCases.map((testCase) => {
      return new Promise((resolve) => {
        const subprocess = spawn(command, testCase, options);

        let accumulatedData = Buffer.alloc(0);

        subprocess.stdout.on('data', common.mustCall((data) => {
          accumulatedData = Buffer.concat([accumulatedData, data]);
        }));

        subprocess.stdout.on('end', () => {
          resolve(accumulatedData.toString().trim().toLowerCase());
        });
      });
    })
  );

  assert.deepStrictEqual([...new Set(results)], [expectedResult]);
})();
