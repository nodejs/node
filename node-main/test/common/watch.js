'use strict';
const common = require('./index.js');
const tmpdir = require('./tmpdir.js');
const fixtures = require('./fixtures.js');
const { writeFileSync, readdirSync, readFileSync, renameSync, unlinkSync } = require('node:fs');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const assert = require('node:assert');
const { setTimeout } = require('node:timers/promises');

function skipIfNoWatch() {
  if (common.isIBMi) {
    common.skip('IBMi does not support `fs.watch()`');
  }

  if (common.isAIX) {
    common.skip('folder watch capability is limited in AIX.');
  }
}

function skipIfNoWatchModeSignals() {
  if (common.isWindows) {
    common.skip('no signals on Windows');
  }

  if (common.isIBMi) {
    common.skip('IBMi does not support `fs.watch()`');
  }

  if (common.isAIX) {
    common.skip('folder watch capability is limited in AIX.');
  }
}

const fixturePaths = {};
const fixtureContent = {};

function refreshForTestRunnerWatch() {
  tmpdir.refresh();
  const files = readdirSync(fixtures.path('test-runner-watch'));
  for (const file of files) {
    const src = fixtures.path('test-runner-watch', file);
    const dest = tmpdir.resolve(file);
    fixturePaths[file] = dest;
    fixtureContent[file] = readFileSync(src, 'utf8');
    writeFileSync(dest, fixtureContent[file]);
  }
}

async function performFileOperation(operation, useRunApi, timeout = 1000) {
  if (useRunApi) {
    const interval = setInterval(() => {
      operation();
      clearInterval(interval);
    }, common.platformTimeout(timeout));
  } else {
    operation();
    await setTimeout(common.platformTimeout(timeout));
  }
}

function assertTestOutput(run, shouldCheckRecursion = false) {
  if (shouldCheckRecursion) {
    assert.doesNotMatch(run, /run\(\) is being called recursively/);
  }
  assert.match(run, /tests 1/);
  assert.match(run, /pass 1/);
  assert.match(run, /fail 0/);
  assert.match(run, /cancelled 0/);
}

async function testRunnerWatch({
  fileToUpdate,
  file,
  action = 'update',
  fileToCreate,
  isolation,
  useRunApi = false,
  cwd = tmpdir.path,
  runnerCwd,
}) {
  const ran1 = Promise.withResolvers();
  const ran2 = Promise.withResolvers();

  let args;
  if (useRunApi) {
    // Use the fixture that calls run() API
    const runner = fixtures.path('test-runner-watch.mjs');
    args = [runner];
    if (file) args.push('--file', file);
    if (runnerCwd) args.push('--cwd', runnerCwd);
    if (isolation) args.push('--isolation', isolation);
  } else {
    // Use CLI --watch --test flags
    args = ['--watch', '--test', '--test-reporter=spec',
            isolation ? `--test-isolation=${isolation}` : '',
            file ? fixturePaths[file] : undefined].filter(Boolean);
  }

  const child = spawn(process.execPath, args,
                      { encoding: 'utf8', stdio: 'pipe', cwd });
  let stdout = '';
  let currentRun = '';
  const runs = [];

  child.stdout.on('data', (data) => {
    stdout += data.toString();
    currentRun += data.toString();
    const testRuns = stdout.match(/duration_ms\s\d+/g);
    if (testRuns?.length >= 1) ran1.resolve();
    if (testRuns?.length >= 2) ran2.resolve();
  });

  const testUpdate = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const content = fixtureContent[fileToUpdate];
    const path = fixturePaths[fileToUpdate];

    if (useRunApi) {
      const interval = setInterval(
        () => writeFileSync(path, content),
        common.platformTimeout(1000),
      );
      await ran2.promise;
      clearInterval(interval);
    } else {
      writeFileSync(path, content);
      await setTimeout(common.platformTimeout(1000));
      await ran2.promise;
    }

    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    for (const run of runs) {
      assertTestOutput(run, useRunApi);
    }
  };

  const testRename = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToRenamePath = tmpdir.resolve(fileToUpdate);
    const newFileNamePath = tmpdir.resolve(`test-renamed-${fileToUpdate}`);

    await performFileOperation(
      () => renameSync(fileToRenamePath, newFileNamePath),
      useRunApi,
    );
    await ran2.promise;

    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    const [firstRun, secondRun] = runs;
    assertTestOutput(firstRun, useRunApi);

    if (action === 'rename2') {
      assert.match(secondRun, /MODULE_NOT_FOUND/);
      return;
    }

    assertTestOutput(secondRun, useRunApi);
  };

  const testDelete = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToDeletePath = tmpdir.resolve(fileToUpdate);

    if (useRunApi) {
      const { existsSync } = require('node:fs');
      const interval = setInterval(() => {
        if (existsSync(fileToDeletePath)) {
          unlinkSync(fileToDeletePath);
        } else {
          ran2.resolve();
          clearInterval(interval);
        }
      }, common.platformTimeout(1000));
      await ran2.promise;
    } else {
      unlinkSync(fileToDeletePath);
      await setTimeout(common.platformTimeout(2000));
      ran2.resolve();
    }

    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    for (const run of runs) {
      assert.doesNotMatch(run, /MODULE_NOT_FOUND/);
    }
  };

  const testCreate = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const newFilePath = tmpdir.resolve(fileToCreate);

    await performFileOperation(
      () => writeFileSync(newFilePath, 'module.exports = {};'),
      useRunApi,
    );
    await ran2.promise;

    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    for (const run of runs) {
      assertTestOutput(run, false);
    }
  };

  action === 'update' && await testUpdate();
  action === 'rename' && await testRename();
  action === 'rename2' && await testRename();
  action === 'delete' && await testDelete();
  action === 'create' && await testCreate();

  return runs;
}


module.exports = {
  skipIfNoWatch,
  skipIfNoWatchModeSignals,
  testRunnerWatch,
  refreshForTestRunnerWatch,
  fixtureContent,
  fixturePaths,
};
