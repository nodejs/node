'use strict';

require('../common');

const deprecated = [
  {
    code: '',
    expected: 0,
  },
  {
    code: '1 one',
    expected: 0,
  },
  {
    code: 'two',
    expected: 0,
  },
  {
    code: {},
    expected: 0,
  },
  {
    code: [],
    expected: 0,
  },
  {
    code: true,
    expected: 1,
  },
  {
    code: false,
    expected: 0,
  },
  {
    code: 2n,
    expected: 0,
    expected_useProcessExitCode: 1,
  },
  {
    code: 2.1,
    expected: 2,
  },
  {
    code: Infinity,
    expected: 0,
  },
  {
    code: NaN,
    expected: 0,
  },
];
const args = deprecated;

if (process.argv[2] === undefined) {
  const { spawnSync } = require('node:child_process');
  const { inspect, debuglog } = require('node:util');
  const { strictEqual } = require('node:assert');

  const debug = debuglog('test');
  const node = process.execPath;
  const test = (index, useProcessExitCode) => {
    const { status: code, stderr } = spawnSync(node, [
      __filename,
      index,
      useProcessExitCode,
    ]);
    debug(`actual: ${code}, ${inspect(args[index])} ${!!useProcessExitCode}`);
    debug(`${stderr}`);

    const expected =
      useProcessExitCode && args[index].expected_useProcessExitCode ?
        args[index].expected_useProcessExitCode :
        args[index].expected;

    strictEqual(code, expected, `actual: ${code}, ${inspect(args[index])}`);
    strictEqual(
      ['[DEP0164]'].some((pattern) => stderr.includes(pattern)),
      true
    );
  };

  for (const index of args.keys()) {
    // Check `process.exit([code])`
    test(index);
    // Check exit with `process.exitCode`
    test(index, true);
  }
} else {
  const index = parseInt(process.argv[2]);
  const useProcessExitCode = process.argv[3] !== 'undefined';
  if (Number.isNaN(index)) {
    return process.exit(100);
  }

  if (useProcessExitCode) {
    process.exitCode = args[index].code;
  } else {
    process.exit(args[index].code);
  }
}
