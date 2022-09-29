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

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

function restart(file) {
  // To avoid flakiness, we save the file repeatedly until test is done
  writeFileSync(file, readFileSync(file));
  const timer = setInterval(() => writeFileSync(file, readFileSync(file)), 1000);
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

  const child = spawn(execPath, ['--watch', '--no-warnings', ...args], { encoding: 'utf8' });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.stdout.on('data', async (data) => {
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
  });

  await once(child, 'exit');
  cancelRestarts?.();
  return { stderr, stdout };
}

let tmpFiles = 0;
function createTmpFile(content = 'console.log("running");') {
  const file = path.join(tmpdir.path, `${tmpFiles++}.js`);
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

tmpdir.refresh();

// Warning: this suite can run safely with concurrency: true
// only if tests do not watch/depend on the same files
describe('watch mode', { concurrency: true, timeout: 60_0000 }, () => {
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

  it('should not watch when running an non-existing file', async () => {
    const file = fixtures.path('watch-mode/non-existing.js');
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 0 });

    assert.match(stderr, /code: 'MODULE_NOT_FOUND'/);
    assert.strictEqual(stdout, `Failed running ${inspect(file)}\n`);
  });

  it('should watch when running an non-existing file - when specified under --watch-path', {
    skip: !common.isOSX && !common.isWindows
  }, async () => {
    const file = fixtures.path('watch-mode/subdir/non-existing.js');
    const watchedFile = fixtures.path('watch-mode/subdir/file.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      watchedFile,
      args: ['--watch-path', fixtures.path('./watch-mode/subdir/'), file],
    });

    assert.strictEqual(stderr, '');
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

  it('should not load --import modules in main process', {
    skip: 'enable once --import is backported',
  }, async () => {
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
});
