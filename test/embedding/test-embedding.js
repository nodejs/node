'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

tmpdir.refresh();
common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);

function resolveBuiltBinary(binary) {
  if (common.isWindows) {
    binary += '.exe';
  }
  return path.join(path.dirname(process.execPath), binary);
}

const binary = resolveBuiltBinary('embedtest');
assert.ok(fs.existsSync(binary));

function runTest(testName, spawn, ...args) {
  process.stdout.write(`Run test: ${testName} ... `);
  spawn(binary, ...args);
  console.log('ok');
}

function runCommonApiTests(apiType) {
  runTest(
    `${apiType}: console.log`,
    spawnSyncAndAssert,
    [apiType, 'console.log(42)'],
    {
      trim: true,
      stdout: '42',
    }
  );

  runTest(
    `${apiType}: console.log non-ascii`,
    spawnSyncAndAssert,
    [apiType, 'console.log(embedVars.nön_ascıı)'],
    {
      trim: true,
      stdout: '🏳️‍🌈',
    }
  );

  runTest(
    `${apiType}: throw new Error()`,
    spawnSyncAndExit,
    [apiType, 'throw new Error()'],
    {
      status: 1,
      signal: null,
    }
  );

  runTest(
    `${apiType}: require("lib/internal/test/binding")`,
    spawnSyncAndExit,
    [apiType, 'require("lib/internal/test/binding")'],
    {
      status: 1,
      signal: null,
    }
  );

  runTest(
    `${apiType}: process.exitCode = 8`,
    spawnSyncAndExit,
    [apiType, 'process.exitCode = 8'],
    {
      status: 8,
      signal: null,
    }
  );

  {
    const fixturePath = JSON.stringify(fixtures.path('exit.js'));
    runTest(
      `${apiType}: require(fixturePath)`,
      spawnSyncAndExit,
      [apiType, `require(${fixturePath})`, 92],
      {
        status: 92,
        signal: null,
      }
    );
  }

  runTest(
    `${apiType}: syntax error`,
    spawnSyncAndExit,
    [apiType, '0syntax_error'],
    {
      status: 1,
      stderr: /SyntaxError: Invalid or unexpected token/,
    }
  );

  // Guarantee NODE_REPL_EXTERNAL_MODULE won't bypass kDisableNodeOptionsEnv
  runTest(
    `${apiType}: check kDisableNodeOptionsEnv`,
    spawnSyncAndExit,
    [apiType, 'require("os")'],
    {
      env: {
        ...process.env,
        NODE_REPL_EXTERNAL_MODULE: 'fs',
      },
    },
    {
      status: 9,
      signal: null,
      stderr: `${binary}: NODE_REPL_EXTERNAL_MODULE can't be used with kDisableNodeOptionsEnv${os.EOL}`,
    }
  );
}

runCommonApiTests('cpp-api');
runCommonApiTests('node-api');

function getReadFileCodeForPath(path) {
  return `(require("fs").readFileSync(${JSON.stringify(path)}, "utf8"))`;
}

function runSnapshotTests(apiType) {
  // Basic snapshot support
  for (const extraSnapshotArgs of [
    [],
    ['--embedder-snapshot-as-file'],
    ['--without-code-cache'],
  ]) {
    // readSync + eval since snapshots don't support userland require() (yet)
    const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
    const blobPath = tmpdir.resolve('embedder-snapshot.blob');
    const buildSnapshotExecArgs = [
      `eval(${getReadFileCodeForPath(snapshotFixture)})`,
      'arg1',
      'arg2',
    ];
    const embedTestBuildArgs = [
      '--embedder-snapshot-blob',
      blobPath,
      '--embedder-snapshot-create',
      ...extraSnapshotArgs,
    ];
    const buildSnapshotArgs = [...buildSnapshotExecArgs, ...embedTestBuildArgs];

    const runSnapshotExecArgs = ['arg3', 'arg4'];
    const embedTestRunArgs = [
      '--embedder-snapshot-blob',
      blobPath,
      ...extraSnapshotArgs,
    ];
    const runSnapshotArgs = [...runSnapshotExecArgs, ...embedTestRunArgs];

    fs.rmSync(blobPath, { force: true });
    runTest(
      `${apiType}: build basic snapshot ${extraSnapshotArgs.join(' ')}`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...buildSnapshotArgs],
      {
        cwd: tmpdir.path,
      }
    );

    runTest(
      `${apiType}: run basic snapshot ${extraSnapshotArgs.join(' ')}`,
      spawnSyncAndAssert,
      [apiType, '--', ...runSnapshotArgs],
      { cwd: tmpdir.path },
      {
        stdout(output) {
          assert.deepStrictEqual(JSON.parse(output), {
            originalArgv: [
              binary,
              '__node_anonymous_main',
              ...buildSnapshotExecArgs,
            ],
            currentArgv: [binary, ...runSnapshotExecArgs],
          });
          return true;
        },
      }
    );
  }

  // Create workers and vm contexts after deserialization
  {
    const snapshotFixture = fixtures.path(
      'snapshot',
      'create-worker-and-vm.js'
    );
    const blobPath = tmpdir.resolve('embedder-snapshot.blob');
    const buildSnapshotArgs = [
      `eval(${getReadFileCodeForPath(snapshotFixture)})`,
      '--embedder-snapshot-blob',
      blobPath,
      '--embedder-snapshot-create',
    ];
    const runEmbeddedArgs = ['--embedder-snapshot-blob', blobPath];

    fs.rmSync(blobPath, { force: true });

    runTest(
      `${apiType}: build create-worker-and-vm snapshot`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...buildSnapshotArgs],
      {
        cwd: tmpdir.path,
      }
    );

    runTest(
      `${apiType}: run create-worker-and-vm snapshot`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...runEmbeddedArgs],
      {
        cwd: tmpdir.path,
      }
    );
  }
}

runSnapshotTests('cpp-api');
runSnapshotTests('snapshot-node-api');

// Node-API specific tests
{
  runTest(
    `node-api: callMe`,
    spawnSyncAndAssert,
    ['node-api', 'function callMe(text) { return text + " you"; }'],
    { stdout: 'called you' }
  );

  runTest(
    `node-api: waitMe`,
    spawnSyncAndAssert,
    [
      'node-api',
      'function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }',
    ],
    { stdout: 'waited you' }
  );

  runTest(
    `node-api: waitPromise`,
    spawnSyncAndAssert,
    [
      'node-api',
      'function waitPromise(text)' +
        '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }',
    ],
    { stdout: 'waited with cheese' }
  );

  runTest(
    `node-api: waitPromise reject`,
    spawnSyncAndAssert,
    [
      'node-api',
      'function waitPromise(text)' +
        '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }',
    ],
    { stdout: 'waited without cheese' }
  );

  runTest(
    `concurrent-node-api: run 12 environments concurrently`,
    spawnSyncAndAssert,
    ['concurrent-node-api', 'myCount = 1'],
    {
      trim: true,
      stdout: '12',
    }
  );

  runTest(
    'multi-env-node-api: run 12 environments in the same thread',
    spawnSyncAndAssert,
    [
      'multi-env-node-api',
      'myCount = 0; function incMyCount() { ++myCount; if (myCount < 5) { setTimeout(incMyCount, 1); } }',
    ],
    {
      trim: true,
      stdout: '60',
    }
  );

  runTest(
    'multi-thread-node-api: run and environment from multiple threads',
    spawnSyncAndAssert,
    [
      'multi-thread-node-api',
      'myCount = 0; function incMyCount() { ++myCount; if (myCount < 5) { setTimeout(incMyCount, 1); } }',
    ],
    {
      trim: true,
      stdout: '5',
    }
  );
}

/*
runTest(
  `modules-node-api: load modules`,
  spawnSyncAndExitWithoutError,
  ['modules-node-api', 'cjs.cjs', 'es6.mjs', ],
  {
    cwd: __dirname,
  }
);
*/
