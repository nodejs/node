'use strict';

require('../common');

const args = [
  {
    code1: 2,
    code2: undefined,
    expected: 2,
  },
  {
    code1: 2,
    code2: null,
    expected: 2,
  },
  {
    code1: 2,
    code2: 0,
    expected: 0,
  },
];

if (process.argv[2] === undefined) {
  const { spawnSync } = require('node:child_process');
  const { strictEqual } = require('node:assert');

  const test = (index) => {
    const { status } = spawnSync(process.execPath, [__filename, index]);
    const expectedCode = args[index].expected;

    strictEqual(status, expectedCode, `actual: ${status}, ${expectedCode}`);
  };

  for (const index of args.keys()) {
    test(index);
  }
} else {
  const index = parseInt(process.argv[2]);

  process.exitCode = args[index].code1;
  process.exit(args[index].code2);
}
