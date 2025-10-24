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

async function testRunnerWatch({
  fileToUpdate,
  file,
  action = 'update',
  fileToCreate,
  isolation,
}) {
  const ran1 = Promise.withResolvers();
  const ran2 = Promise.withResolvers();
  const child = spawn(process.execPath,
                      ['--watch', '--test', '--test-reporter=spec',
                       isolation ? `--test-isolation=${isolation}` : '',
                       file ? fixturePaths[file] : undefined].filter(Boolean),
                      { encoding: 'utf8', stdio: 'pipe', cwd: tmpdir.path });
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
    writeFileSync(path, content);
    await setTimeout(common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    for (const run of runs) {
      assert.match(run, /tests 1/);
      assert.match(run, /pass 1/);
      assert.match(run, /fail 0/);
      assert.match(run, /cancelled 0/);
    }
  };

  const testRename = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToRenamePath = tmpdir.resolve(fileToUpdate);
    const newFileNamePath = tmpdir.resolve(`test-renamed-${fileToUpdate}`);
    renameSync(fileToRenamePath, newFileNamePath);
    await setTimeout(common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    for (const run of runs) {
      assert.match(run, /tests 1/);
      assert.match(run, /pass 1/);
      assert.match(run, /fail 0/);
      assert.match(run, /cancelled 0/);
    }
  };

  const testDelete = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToDeletePath = tmpdir.resolve(fileToUpdate);
    unlinkSync(fileToDeletePath);
    await setTimeout(common.platformTimeout(2000));
    ran2.resolve();
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
    writeFileSync(newFilePath, 'module.exports = {};');
    await setTimeout(common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    for (const run of runs) {
      assert.match(run, /tests 1/);
      assert.match(run, /pass 1/);
      assert.match(run, /fail 0/);
      assert.match(run, /cancelled 0/);
    }
  };

  action === 'update' && await testUpdate();
  action === 'rename' && await testRename();
  action === 'delete' && await testDelete();
  action === 'create' && await testCreate();
}


module.exports = {
  skipIfNoWatch,
  skipIfNoWatchModeSignals,
  testRunnerWatch,
  refreshForTestRunnerWatch,
};
