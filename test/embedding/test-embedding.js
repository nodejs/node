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
const fs = require('fs');

tmpdir.refresh();
common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);

const binary = common.resolveBuiltBinary('embedtest');

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
    },
  );

  runTest(
    `${apiType}: console.log non-ascii`,
    spawnSyncAndAssert,
    [apiType, 'console.log(embedVars.nön_ascıı)'],
    {
      trim: true,
      stdout: '🏳️‍🌈',
    },
  );

  runTest(
    `${apiType}: throw new Error()`,
    spawnSyncAndExit,
    [apiType, 'throw new Error()'],
    {
      status: 1,
      signal: null,
    },
  );

  runTest(
    `${apiType}: require("lib/internal/test/binding")`,
    spawnSyncAndExit,
    [apiType, 'require("lib/internal/test/binding")'],
    {
      status: 1,
      signal: null,
    },
  );

  runTest(
    `${apiType}: process.exitCode = 8`,
    spawnSyncAndExit,
    [apiType, 'process.exitCode = 8'],
    {
      status: 8,
      signal: null,
    },
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
      },
    );
  }

  runTest(
    `${apiType}: syntax error`,
    spawnSyncAndExit,
    [apiType, '0syntax_error'],
    {
      status: 1,
      stderr: /SyntaxError: Invalid or unexpected token/,
    },
  );

  fs.rmSync(blobPath, { force: true });

  spawnSyncAndExitWithoutError(
    binary,
    [ '--', ...buildSnapshotArgs ],
    { cwd: tmpdir.path });
  spawnSyncAndExitWithoutError(
    binary,
    [ '--', ...runEmbeddedArgs ],
    { cwd: tmpdir.path });
}

// Guarantee NODE_REPL_EXTERNAL_MODULE won't bypass kDisableNodeOptionsEnv
if (!process.config.variables.node_without_node_options) {
  spawnSyncAndExit(
    binary,
    ['require("os")'],
    {
      env: {
        ...process.env,
        NODE_REPL_EXTERNAL_MODULE: 'fs',
      },
    },
    {
      status: 9,
      signal: null,
      trim: true,
      stderr:
        `${binary}: NODE_REPL_EXTERNAL_MODULE can't be used with` +
        ' kDisableNodeOptionsEnv',
    },
  );
}

runCommonApiTests('cpp-api');

function getReadFileCodeForPath(path) {
  return `(require("fs").readFileSync(${JSON.stringify(path)}, "utf8"))`;
}

function runSnapshotTests(apiType) {
  // Basic snapshot support
  for (const extraSnapshotArgs of [
    [], ['--embedder-snapshot-as-file'], ['--without-code-cache'],
  ]) {
    // readSync + eval since snapshots don't support userland require() (yet)
    const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
    const blobPath = tmpdir.resolve('embedder-snapshot.blob');
    const buildSnapshotExecArgs = [
      `eval(${getReadFileCodeForPath(snapshotFixture)})`, 'arg1', 'arg2',
    ];
    const embedTestBuildArgs = [
      '--embedder-snapshot-blob', blobPath, '--embedder-snapshot-create',
      ...extraSnapshotArgs,
    ];
    const buildSnapshotArgs = [
      ...buildSnapshotExecArgs,
      ...embedTestBuildArgs,
    ];

    const runSnapshotExecArgs = [
      'arg3', 'arg4',
    ];
    const embedTestRunArgs = [
      '--embedder-snapshot-blob', blobPath,
      ...extraSnapshotArgs,
    ];
    const runSnapshotArgs = [
      ...runSnapshotExecArgs,
      ...embedTestRunArgs,
    ];

    fs.rmSync(blobPath, { force: true });

    runTest(
      `${apiType}: build basic snapshot ${extraSnapshotArgs.join(' ')}`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...buildSnapshotArgs],
      {
        cwd: tmpdir.path,
      },
    );

    runTest(
      `${apiType}: run basic snapshot ${extraSnapshotArgs.join(' ')}`,
      spawnSyncAndAssert,
      [apiType, '--', ...runSnapshotArgs],
      { cwd: tmpdir.path },
      {
        stdout: common.mustCall((output) => {
          assert.deepStrictEqual(JSON.parse(output), {
            originalArgv: [
              binary,
              '__node_anonymous_main',
              ...buildSnapshotExecArgs,
            ],
            currentArgv: [binary, ...runSnapshotExecArgs],
          });
          return true;
        }),
      },
    );
  }

  // Create workers and vm contexts after deserialization
  {
    const snapshotFixture = fixtures.path('snapshot', 'create-worker-and-vm.js');
    const blobPath = tmpdir.resolve('embedder-snapshot.blob');
    const buildSnapshotArgs = [
      `eval(${getReadFileCodeForPath(snapshotFixture)})`,
      '--embedder-snapshot-blob', blobPath, '--embedder-snapshot-create',
    ];
    const runEmbeddedArgs = [
      '--embedder-snapshot-blob', blobPath,
    ];

    fs.rmSync(blobPath, { force: true });

    runTest(
      `${apiType}: build create-worker-and-vm snapshot`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...buildSnapshotArgs],
      {
        cwd: tmpdir.path,
      },
    );

    runTest(
      `${apiType}: run create-worker-and-vm snapshot`,
      spawnSyncAndExitWithoutError,
      [apiType, '--', ...runEmbeddedArgs],
      {
        cwd: tmpdir.path,
      },
    );
  }
}

runSnapshotTests('cpp-api');

// C-API specific tests
function runCApiTests(apiType) {
  runTest(
    `${apiType}-nodejs-main: run Node.js CLI`,
    spawnSyncAndAssert,
    [`${apiType}-nodejs-main`, '--eval', 'console.log("Hello World")'],
    {
      trim: true,
      stdout: 'Hello World',
    },
  );
}

runCApiTests('c-api');
