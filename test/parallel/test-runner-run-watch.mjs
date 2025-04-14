import * as common from '../common/index.mjs';
import { describe, it, beforeEach, run } from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { writeFileSync, renameSync, unlinkSync, existsSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import { join } from 'node:path';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

// This test updates these files repeatedly,
// Reading them from disk is unreliable due to race conditions.
const fixtureContent = {
  'dependency.js': 'module.exports = {};',
  'dependency.mjs': 'export const a = 1;',
  'test.js': `
const test = require('node:test');
require('./dependency.js');
import('./dependency.mjs');
import('data:text/javascript,');
test('test has ran');`,
};

let fixturePaths;

function refresh() {
  tmpdir.refresh();

  fixturePaths = Object.keys(fixtureContent)
    .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
  Object.entries(fixtureContent)
    .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));
}

const runner = join(import.meta.dirname, '..', 'fixtures', 'test-runner-watch.mjs');

async function testWatch(
  {
    fileToUpdate,
    file,
    action = 'update',
    cwd = tmpdir.path,
    fileToCreate,
    runnerCwd,
    isolation
  }
) {
  const ran1 = Promise.withResolvers();
  const ran2 = Promise.withResolvers();
  const args = [runner];
  if (file) args.push('--file', file);
  if (runnerCwd) args.push('--cwd', runnerCwd);
  if (isolation) args.push('--isolation', isolation);
  const child = spawn(process.execPath,
                      args,
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
    const interval = setInterval(() => writeFileSync(path, content), common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    clearInterval(interval);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    for (const run of runs) {
      assert.doesNotMatch(run, /run\(\) is being called recursively/);
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
    const interval = setInterval(() => {
      renameSync(fileToRenamePath, newFileNamePath);
      clearInterval(interval);
    }, common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    child.kill();
    await once(child, 'exit');

    assert.strictEqual(runs.length, 2);

    const [firstRun, secondRun] = runs;
    assert.match(firstRun, /tests 1/);
    assert.match(firstRun, /pass 1/);
    assert.match(firstRun, /fail 0/);
    assert.match(firstRun, /cancelled 0/);
    assert.doesNotMatch(firstRun, /run\(\) is being called recursively/);

    if (action === 'rename2') {
      assert.match(secondRun, /MODULE_NOT_FOUND/);
      return;
    }

    assert.match(secondRun, /tests 1/);
    assert.match(secondRun, /pass 1/);
    assert.match(secondRun, /fail 0/);
    assert.match(secondRun, /cancelled 0/);
    assert.doesNotMatch(secondRun, /run\(\) is being called recursively/);
  };

  const testDelete = async () => {
    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToDeletePath = tmpdir.resolve(fileToUpdate);
    const interval = setInterval(() => {
      if (existsSync(fileToDeletePath)) {
        unlinkSync(fileToDeletePath);
      } else {
        ran2.resolve();
        clearInterval(interval);
      }
    }, common.platformTimeout(1000));
    await ran2.promise;
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
    const interval = setInterval(
      () => {
        writeFileSync(
          newFilePath,
          'module.exports = {};'
        );
        clearInterval(interval);
      },
      common.platformTimeout(1000)
    );
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
  action === 'rename2' && await testRename();
  action === 'delete' && await testDelete();
  action === 'create' && await testCreate();

  return runs;
}

describe('test runner watch mode', () => {
  beforeEach(refresh);
  it('should run tests repeatedly', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'test.js' });
  });

  it('should run tests with dependency repeatedly', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'dependency.js' });
  });

  it('should run tests with ESM dependency', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'dependency.mjs' });
  });

  it('should support running tests without a file', async () => {
    await testWatch({ fileToUpdate: 'test.js' });
  });

  it('should support a watched test file rename', async () => {
    await testWatch({ fileToUpdate: 'test.js', action: 'rename' });
  });

  it('should not throw when deleting a watched test file', async () => {
    await testWatch({ fileToUpdate: 'test.js', action: 'delete' });
  });

  it('should run tests with dependency repeatedly in a different cwd', async () => {
    await testWatch({
      file: join(tmpdir.path, 'test.js'),
      fileToUpdate: 'dependency.js',
      cwd: import.meta.dirname,
      action: 'rename2'
    });
  });

  it('should handle renames in a different cwd', async () => {
    await testWatch({
      file: join(tmpdir.path, 'test.js'),
      fileToUpdate: 'test.js',
      cwd: import.meta.dirname,
      action: 'rename2'
    });
  });

  it(
    'should run new tests when a new file is created in the watched directory',
    async () => {
      await testWatch({ action: 'create', fileToCreate: 'new-test-file.test.js' });
    });

  // This test is flaky by its nature as it relies on the timing of 2 different runs
  // considering the number of digits in the duration_ms is 9
  // the chances of having the same duration_ms are very low
  // but not impossible
  // In case of costant failures, consider increasing the number of tests
  it('should recalculate the run duration on a watch restart', async () => {
    const testRuns = await testWatch({ file: 'test.js', fileToUpdate: 'test.js' });
    const durations = testRuns.map((run) => {
      const runDuration = run.match(/# duration_ms\s([\d.]+)/);
      return runDuration;
    });
    assert.notDeepStrictEqual(durations[0][1], durations[1][1]);
  });

  describe('test runner watch mode with different cwd', () => {
    it(
      'should execute run using a different cwd for the runner than the process cwd',
      async () => {
        await testWatch(
          {
            fileToUpdate: 'test.js',
            action: 'rename',
            cwd: import.meta.dirname,
            runnerCwd: tmpdir.path
          }
        );
      });

    it(
      'should execute run using a different cwd for the runner than the process cwd with isolation process',
      async () => {
        await testWatch(
          {
            fileToUpdate: 'test.js',
            action: 'rename',
            cwd: import.meta.dirname,
            runnerCwd: tmpdir.path,
            isolation: 'process'
          }
        );
      });

    it('should run with different cwd while in watch mode', async () => {
      const controller = new AbortController();
      const stream = run({
        cwd: tmpdir.path,
        watch: true,
        signal: controller.signal,
      }).on('data', function({ type }) {
        if (type === 'test:watch:drained') {
          stream.removeAllListeners('test:fail');
          stream.removeAllListeners('test:pass');
          controller.abort();
        }
      });

      stream.on('test:fail', common.mustNotCall());
      stream.on('test:pass', common.mustCall(1));
      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });

    it('should run with different cwd while in watch mode and isolation "none"', async () => {
      const controller = new AbortController();
      const stream = run({
        cwd: tmpdir.path,
        watch: true,
        signal: controller.signal,
        isolation: 'none',
      }).on('data', function({ type }) {
        if (type === 'test:watch:drained') {
          stream.removeAllListeners('test:fail');
          stream.removeAllListeners('test:pass');
          controller.abort();
        }
      });

      stream.on('test:fail', common.mustNotCall());
      stream.on('test:pass', common.mustCall(1));
      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });

    it('should run with different cwd while in watch mode and isolation "process"', async () => {
      const controller = new AbortController();
      const stream = run({
        cwd: tmpdir.path,
        watch: true,
        signal: controller.signal,
        isolation: 'process',
      }).on('data', function({ type }) {
        if (type === 'test:watch:drained') {
          stream.removeAllListeners('test:fail');
          stream.removeAllListeners('test:pass');
          controller.abort();
        }
      });

      stream.on('test:fail', common.mustNotCall());
      stream.on('test:pass', common.mustCall(1));
      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });
  });
});
