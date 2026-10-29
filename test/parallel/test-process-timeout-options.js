'use strict';

// Tests the validation of --process-timeout and of the options that cannot be
// combined with it.

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');

// Long enough for the process to exit on its own.
for (const args of [
  ['--process-timeout=600000ms'],
  ['--process-timeout=30s'],
  ['--process-timeout=5m'],
  ['--process-timeout', '1h'],
]) {
  spawnSyncAndAssert(process.execPath, [...args, '-e', ''], {
    stdout: '',
    stderr: '',
  });
}

for (const value of [
  '30', '0s', '0ms', '1.5s', '-1s', '1d', 's', 'abc', ' 1s', '1s ',
  '99999999999999999999h',
]) {
  spawnSyncAndExit(process.execPath, [`--process-timeout=${value}`, '-e', ''], {
    status: 9,
    signal: null,
    trim: true,
    stderr: `${process.execPath}: invalid value for --process-timeout: ` +
            `'${value}'. Expected a positive integer followed by a unit ` +
            '(ms, s, m or h), e.g. 30s',
  });
}

spawnSyncAndExit(process.execPath, ['--report-on-process-timeout', '-e', ''], {
  status: 9,
  signal: null,
  trim: true,
  stderr: `${process.execPath}: --report-on-process-timeout must be used with ` +
          '--process-timeout',
});

if (!process.config.variables.node_without_node_options) {
  for (const option of ['--process-timeout=1s', '--report-on-process-timeout']) {
    spawnSyncAndExit(process.execPath, ['-e', ''], {
      env: { ...process.env, NODE_OPTIONS: option },
    }, {
      status: 9,
      signal: null,
      trim: true,
      stderr: `${process.execPath}: ${option.replace(/=.*/, '=')} is not ` +
              'allowed in NODE_OPTIONS',
    });
  }

  // Conflicts are detected across option sources. The inspector options are
  // the only conflicting options allowed in NODE_OPTIONS, and they only exist
  // when Node.js is built with the inspector.
  if (common.hasInspector) {
    spawnSyncAndExit(process.execPath, ['--process-timeout=1s', '-e', ''], {
      env: { ...process.env, NODE_OPTIONS: '--inspect=0' },
    }, {
      status: 9,
      signal: null,
      trim: true,
      stderr: `${process.execPath}: either --process-timeout or --inspect can be ` +
              'used, not both',
    });
  }
}

const conflicts = [
  [['--run', 'test'], '--run'],
  [['--build-snapshot', 'entry.js'], '--build-snapshot'],
];
// Without the inspector, these options are rejected as bad options instead.
if (common.hasInspector) {
  conflicts.push(
    [['--inspect=0'], '--inspect'],
    [['--inspect-brk=0'], '--inspect-brk'],
    [['--inspect-brk-node=0'], '--inspect-brk-node'],
    [['--inspect-wait=0'], '--inspect-wait'],
    [['--inspect-port=0'], '--inspect-port'],
    [['--debug-port=0'], '--inspect-port'],
    [['--inspect-publish-uid=http'], '--inspect-publish-uid'],
  );
}

for (const [args, conflict] of conflicts) {
  spawnSyncAndExit(process.execPath, ['--process-timeout=1s', ...args], {
    status: 9,
    signal: null,
    trim: true,
    stderr: `${process.execPath}: either --process-timeout or ${conflict} can ` +
            'be used, not both',
  });
}

spawnSyncAndExit(process.execPath, ['--process-timeout=1s', 'inspect', 'app.js'], {
  status: 9,
  signal: null,
  trim: true,
  stderr: `${process.execPath}: --process-timeout cannot be used with ` +
          '`node inspect`',
});

// --process-timeout applies to the whole process, so Workers do not accept it.
assert.throws(() => {
  new Worker('', { eval: true, execArgv: ['--process-timeout=1s'] });
}, {
  code: 'ERR_WORKER_INVALID_EXEC_ARGV',
});
