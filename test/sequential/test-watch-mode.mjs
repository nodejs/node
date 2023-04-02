import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync, readFileSync } from 'node:fs';
import { inspect } from 'node:util';
import { once } from 'node:events';
import { createInterface } from 'node:readline/promises';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const supportsRecursive = common.isOSX || common.isWindows;
let disableRestart = false;

function restart(file) {
  // To avoid flakiness, we save the file repeatedly until test is done
  writeFileSync(file, readFileSync(file));
  const timer = setInterval(() => {
    if (!disableRestart) {
      writeFileSync(file, readFileSync(file));
    }
  }, common.platformTimeout(1000));
  return () => clearInterval(timer);
}

async function spawnWithRestarts({
  args,
  file,
  watchedFile = file,
  restarts = 1,
  isReady,
}) {
  args ??= [file];
  const printedArgs = inspect(args.slice(args.indexOf(file)).join(' '));
  isReady ??= (data) => Boolean(data.match(new RegExp(`(Failed|Completed) running ${printedArgs.replace(/\\/g, '\\\\')}`, 'g'))?.length);

  let stderr = '';
  let stdout = '';
  let cancelRestarts;

  disableRestart = true;
  const child = spawn(execPath, ['--watch', '--no-warnings', ...args], { encoding: 'utf8' });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.stdout.on('data', async (data) => {
    if (data.toString().includes('Restarting')) {
      disableRestart = true;
    }
    stdout += data;
    const restartsCount = stdout.match(new RegExp(`Restarting ${printedArgs.replace(/\\/g, '\\\\')}`, 'g'))?.length ?? 0;
    if (restarts === 0 || !isReady(data.toString())) {
      return;
    }
    if (restartsCount >= restarts) {
      cancelRestarts?.();
      child.kill();
      return;
    }
    cancelRestarts ??= restart(watchedFile);
    if (isReady(data.toString())) {
      disableRestart = false;
    }
  });

  await once(child, 'exit');
  cancelRestarts?.();
  return { stderr, stdout };
}

let tmpFiles = 0;
function createTmpFile(content = 'console.log("running");', ext = '.js') {
  const file = path.join(tmpdir.path, `${tmpFiles++}${ext}`);
  writeFileSync(file, content);
  return file;
}

function assertRestartedCorrectly({ stdout, messages: { inner, completed, restarted } }) {
  const lines = stdout.split(/\r?\n/).filter(Boolean);

  const start = [inner, completed, restarted].filter(Boolean);
  const end = [restarted, inner, completed].filter(Boolean);
  assert.deepStrictEqual(lines.slice(0, start.length), start);
  assert.deepStrictEqual(lines.slice(-end.length), end);
}

async function failWriteSucceed({ file, watchedFile }) {
  const child = spawn(execPath, ['--watch', '--no-warnings', file], { encoding: 'utf8' });

  try {
    // Break the chunks into lines
    for await (const data of createInterface({ input: child.stdout })) {
      if (data.startsWith('Completed running')) {
        break;
      }
      if (data.startsWith('Failed running')) {
        writeFileSync(watchedFile, 'console.log("test has ran");');
      }
    }
  } finally {
    child.kill();
  }
}

tmpdir.refresh();

// Warning: this suite cannot run safely with concurrency: true
// because of the disableRestart flag used for controlling restarts
describe('watch mode', { concurrency: false, timeout: 60_000 }, () => {
  it('should watch changes to a file - event loop ended', async () => {
    const file = createTmpFile();
    const { stderr, stdout } = await spawnWithRestarts({ file });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: 'running', completed: `Completed running ${inspect(file)}`, restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch changes to a failing file', async () => {
    const file = fixtures.path('watch-mode/failing.js');
    const { stderr, stdout } = await spawnWithRestarts({ file });

    // Use match first to pretty print diff on failure
    assert.match(stderr, /Error: fails\r?\n/);
    // Test that failures happen once per restart
    assert(stderr.match(/Error: fails\r?\n/g).length >= 2);
    assertRestartedCorrectly({
      stdout,
      messages: { completed: `Failed running ${inspect(file)}`, restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch changes to a file with watch-path', {
    skip: !supportsRecursive,
  }, async () => {
    const file = createTmpFile();
    const watchedFile = fixtures.path('watch-mode/subdir/file.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile,
      args: ['--watch-path', fixtures.path('./watch-mode/subdir'), file],
    });
    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: 'running', completed: `Completed running ${inspect(file)}`, restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch when running an non-existing file - when specified under --watch-path', {
    skip: !supportsRecursive
  }, async () => {
    const file = fixtures.path('watch-mode/subdir/non-existing.js');
    const watchedFile = fixtures.path('watch-mode/subdir/file.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile,
      args: ['--watch-path', fixtures.path('./watch-mode/subdir/'), file],
    });

    assert.match(stderr, /Error: Cannot find module/);
    assert(stderr.match(/Error: Cannot find module/g).length >= 2);

    assertRestartedCorrectly({
      stdout,
      messages: { completed: `Failed running ${inspect(file)}`, restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch when running an non-existing file - when specified under --watch-path with equals', {
    skip: !supportsRecursive
  }, async () => {
    const file = fixtures.path('watch-mode/subdir/non-existing.js');
    const watchedFile = fixtures.path('watch-mode/subdir/file.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile,
      args: [`--watch-path=${fixtures.path('./watch-mode/subdir/')}`, file],
    });

    assert.match(stderr, /Error: Cannot find module/);
    assert(stderr.match(/Error: Cannot find module/g).length >= 2);
    assertRestartedCorrectly({
      stdout,
      messages: { completed: `Failed running ${inspect(file)}`, restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch changes to a file - event loop blocked', async () => {
    const file = fixtures.path('watch-mode/event_loop_blocked.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      isReady: (data) => data.startsWith('running'),
    });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: 'running', restarted: `Restarting ${inspect(file)}` },
    });
  });

  it('should watch changes to dependencies - cjs', async () => {
    const file = fixtures.path('watch-mode/dependant.js');
    const dependency = fixtures.path('watch-mode/dependency.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile: dependency,
    });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: '{}', restarted: `Restarting ${inspect(file)}`, completed: `Completed running ${inspect(file)}` },
    });
  });

  it('should watch changes to dependencies - esm', async () => {
    const file = fixtures.path('watch-mode/dependant.mjs');
    const dependency = fixtures.path('watch-mode/dependency.mjs');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile: dependency,
    });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: '{}', restarted: `Restarting ${inspect(file)}`, completed: `Completed running ${inspect(file)}` },
    });
  });

  it('should restart multiple times', async () => {
    const file = createTmpFile();
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 3 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.match(new RegExp(`Restarting ${inspect(file).replace(/\\/g, '\\\\')}`, 'g')).length, 3);
  });

  it('should pass arguments to file', async () => {
    const file = fixtures.path('watch-mode/parse_args.js');
    const random = Date.now().toString();
    const args = [file, '--random', random];
    const { stderr, stdout } = await spawnWithRestarts({ file, args });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { inner: random, restarted: `Restarting ${inspect(args.join(' '))}`, completed: `Completed running ${inspect(args.join(' '))}` },
    });
  });

  it('should not load --require modules in main process', async () => {
    const file = createTmpFile('');
    const required = fixtures.path('watch-mode/process_exit.js');
    const args = ['--require', required, file];
    const { stderr, stdout } = await spawnWithRestarts({ file, args });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { restarted: `Restarting ${inspect(file)}`, completed: `Completed running ${inspect(file)}` },
    });
  });

  it('should not load --import modules in main process', async () => {
    const file = createTmpFile('');
    const imported = fixtures.fileURL('watch-mode/process_exit.js');
    const args = ['--import', imported, file];
    const { stderr, stdout } = await spawnWithRestarts({ file, args });

    assert.strictEqual(stderr, '');
    assertRestartedCorrectly({
      stdout,
      messages: { restarted: `Restarting ${inspect(file)}`, completed: `Completed running ${inspect(file)}` },
    });
  });

  // TODO: Remove skip after https://github.com/nodejs/node/pull/45271 lands
  it('should not watch when running an missing file', {
    skip: !supportsRecursive
  }, async () => {
    const nonExistingfile = path.join(tmpdir.path, `${tmpFiles++}.js`);
    await failWriteSucceed({ file: nonExistingfile, watchedFile: nonExistingfile });
  });

  it('should not watch when running an missing mjs file', {
    skip: !supportsRecursive
  }, async () => {
    const nonExistingfile = path.join(tmpdir.path, `${tmpFiles++}.mjs`);
    await failWriteSucceed({ file: nonExistingfile, watchedFile: nonExistingfile });
  });

  it('should watch changes to previously missing dependency', {
    skip: !supportsRecursive
  }, async () => {
    const dependency = path.join(tmpdir.path, `${tmpFiles++}.js`);
    const relativeDependencyPath = `./${path.basename(dependency)}`;
    const dependant = createTmpFile(`console.log(require('${relativeDependencyPath}'))`);

    await failWriteSucceed({ file: dependant, watchedFile: dependency });
  });

  it('should watch changes to previously missing ESM dependency', {
    skip: !supportsRecursive
  }, async () => {
    const dependency = path.join(tmpdir.path, `${tmpFiles++}.mjs`);
    const relativeDependencyPath = `./${path.basename(dependency)}`;
    const dependant = createTmpFile(`import '${relativeDependencyPath}'`, '.mjs');

    await failWriteSucceed({ file: dependant, watchedFile: dependency });
  });

  it('should preserve output when --watch-preserve-output flag is passed', async () => {
    const file = createTmpFile();
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      args: ['--watch-preserve-output', file],
    });

    assert.strictEqual(stderr, '');
    // Checks if the existing output is preserved
    assertRestartedCorrectly({
      stdout,
      messages: {
        inner: 'running',
        restarted: `Restarting ${inspect(file)}`,
        completed: `Completed running ${inspect(file)}`,
      },
    });
    // Remove the first 3 lines from stdout
    const lines = stdout.split(/\r?\n/).filter(Boolean).slice(3);
    assert.deepStrictEqual(lines, [
      'running',
      `Completed running ${inspect(file)}`,
    ]);
  });
});
