'use strict';

// Tests that `node inspect` help text is printed when
// `--help` / `-h` is passed and there is no script,
// or when there are no additional arguments.
const common = require('../common');
common.skipIfInspectorDisabled();

const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');

const usageRegex = /^Usage: .+ inspect .*debugger\.html/ms;

// --help / -h prints the usage to stdout and exits with code 0,
// for both interactive and probe-mode invocations.
const helpArgs = [
  ['inspect', '--help'],
  ['inspect', '-h'],
  ['inspect', '--probe', '--help'],
  ['inspect', '--probe', '-h'],
];

for (const args of helpArgs) {
  spawnSyncAndAssert(process.execPath, args, {
    stdout: usageRegex,
  });
}

// `node inspect` with no args prints the usage to stderr and exits
// with kInvalidCommandLineArgument (9).
spawnSyncAndExit(process.execPath, ['inspect'], {
  status: 9,
  signal: null,
  stderr: usageRegex,
});
