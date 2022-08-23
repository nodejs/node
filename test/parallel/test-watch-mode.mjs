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
import { setTimeout } from 'node:timers/promises';
import { NodeInstance } from '../common/inspector-helper.js';


if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

async function spawnWithRestarts({
  args,
  file,
  restarts,
  startedPredicate,
  restartMethod,
}) {
  args ??= [file];
  const printedArgs = inspect(args.slice(args.indexOf(file)).join(' '));
  startedPredicate ??= (data) => Boolean(data.match(new RegExp(`(Failed|Completed) running ${printedArgs.replace(/\\/g, '\\\\')}`, 'g'))?.length);
  restartMethod ??= () => writeFileSync(file, readFileSync(file));

  let stderr = '';
  let stdout = '';
  let restartCount = 0;
  let completedStart = false;
  let finished = false;

  const child = spawn(execPath, ['--watch', '--no-warnings', ...args], { encoding: 'utf8' });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.stdout.on('data', async (data) => {
    if (finished) return;
    stdout += data;
    const restartMessages = stdout.match(new RegExp(`Restarting ${printedArgs.replace(/\\/g, '\\\\')}`, 'g'))?.length ?? 0;
    completedStart = completedStart || startedPredicate(data.toString());
    if (restartMessages >= restarts && completedStart) {
      finished = true;
      child.kill();
      return;
    }
    if (restartCount <= restartMessages && completedStart) {
      await setTimeout(restartCount > 0 ? 1000 : 50, { ref: false }); // Prevent throttling
      restartCount++;
      completedStart = false;
      restartMethod();
    }
  });

  await Promise.race([once(child, 'exit'), once(child, 'error')]);
  return { stderr, stdout };
}

let tmpFiles = 0;
function createTmpFile(content = 'console.log("running");') {
  const file = path.join(tmpdir.path, `${tmpFiles++}.js`);
  writeFileSync(file, content);
  return file;
}

function removeGraceMessage(stdout, file) {
  // Remove the message in case restart took long to avoid flakiness
  return stdout
    .replaceAll('Waiting for graceful termination...', '')
    .replaceAll(`Gracefully restarted ${inspect(file)}`, '');
}

tmpdir.refresh();

// Warning: this suite can run safely with concurrency: true
// only if tests do not watch/depend on the same files
describe('watch mode', { concurrency: false, timeout: 60_0000 }, () => {
  it('should watch changes to a file - event loop ended', async () => {
    const file = createTmpFile();
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 1 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [
      'running', `Completed running ${inspect(file)}`, `Restarting ${inspect(file)}`,
      'running', `Completed running ${inspect(file)}`, '',
    ].join('\n'));
  });

  it('should watch changes to a failing file', async () => {
    const file = fixtures.path('watch-mode/failing.js');
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 1 });

    assert.match(stderr, /Error: fails\r?\n/);
    assert.strictEqual(stderr.match(/Error: fails\r?\n/g).length, 2);
    assert.strictEqual(removeGraceMessage(stdout, file), [`Failed running ${inspect(file)}`, `Restarting ${inspect(file)}`,
                                                          `Failed running ${inspect(file)}`, ''].join('\n'));
  });

  it('should not watch when running an non-existing file', async () => {
    const file = fixtures.path('watch-mode/non-existing.js');
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 0, restartMethod: () => {} });

    assert.match(stderr, /code: 'MODULE_NOT_FOUND'/);
    assert.strictEqual(stdout, [`Failed running ${inspect(file)}`, ''].join('\n'));
  });

  it('should watch when running an non-existing file - when specified under --watch-path', {
    skip: !common.isOSX && !common.isWindows
  }, async () => {
    const file = fixtures.path('watch-mode/subdir/non-existing.js');
    const watched = fixtures.path('watch-mode/subdir/file.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      args: ['--watch-path', fixtures.path('./watch-mode/subdir/'), file],
      restarts: 1,
      restartMethod: () => writeFileSync(watched, readFileSync(watched))
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [`Failed running ${inspect(file)}`, `Restarting ${inspect(file)}`,
                                                          `Failed running ${inspect(file)}`, ''].join('\n'));
  });

  it('should watch changes to a file - event loop blocked', async () => {
    const file = fixtures.path('watch-mode/infinite-loop.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      restarts: 2,
      startedPredicate: (data) => data.startsWith('running'),
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file),
                       ['running', `Restarting ${inspect(file)}`, 'running', `Restarting ${inspect(file)}`, 'running', ''].join('\n'));
  });

  it('should watch changes to dependencies - cjs', async () => {
    const file = fixtures.path('watch-mode/dependant.js');
    const dependency = fixtures.path('watch-mode/dependency.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      restarts: 1,
      restartMethod: () => writeFileSync(dependency, readFileSync(dependency)),
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [
      '{}', `Completed running ${inspect(file)}`, `Restarting ${inspect(file)}`,
      '{}', `Completed running ${inspect(file)}`, '',
    ].join('\n'));
  });

  it('should watch changes to dependencies - esm', async () => {
    const file = fixtures.path('watch-mode/dependant.mjs');
    const dependency = fixtures.path('watch-mode/dependency.mjs');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      restarts: 1,
      restartMethod: () => writeFileSync(dependency, readFileSync(dependency)),
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [
      '{}', `Completed running ${inspect(file)}`, `Restarting ${inspect(file)}`,
      '{}', `Completed running ${inspect(file)}`, '',
    ].join('\n'));
  });

  it('should restart multiple times', async () => {
    const file = createTmpFile();
    const { stderr, stdout } = await spawnWithRestarts({ file, restarts: 3 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.match(new RegExp(`Restarting ${inspect(file).replace(/\\/g, '\\\\')}`, 'g')).length, 3);
  });

  it('should gracefully wait when restarting', { skip: common.isWindows }, async () => {
    const file = fixtures.path('watch-mode/graceful-sigterm.js');
    const { stderr, stdout } = await spawnWithRestarts({
      file,
      restarts: 1,
      startedPredicate: (data) => data.startsWith('running'),
    });

    // This message appearing is very flaky depending on a race between the
    // inner process and the outer process. it is acceptable for the message not to appear
    // as long as the SIGTERM handler is respected.
    if (stdout.includes('Waiting for graceful termination...')) {
      assert.strictEqual(stdout, ['running', `Restarting ${inspect(file)}`, 'Waiting for graceful termination...',
                                  'exiting gracefully', `Gracefully restarted ${inspect(file)}`, 'running', ''].join('\n'));
    } else {
      assert.strictEqual(stdout, ['running', `Restarting ${inspect(file)}`, 'exiting gracefully', 'running', ''].join('\n'));
    }
    assert.strictEqual(stderr, '');
  });

  it('should pass arguments to file', async () => {
    const file = fixtures.path('watch-mode/parse_args.js');
    const random = Date.now().toString();
    const args = [file, '--random', random];
    const { stderr, stdout } = await spawnWithRestarts({ file, args, restarts: 1 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, args.join(' ')), [
      random, `Completed running ${inspect(args.join(' '))}`, `Restarting ${inspect(args.join(' '))}`,
      random, `Completed running ${inspect(args.join(' '))}`, '',
    ].join('\n'));
  });

  it('should not load --require modules in main process', async () => {
    const file = createTmpFile('');
    const required = fixtures.path('watch-mode/process_exit.js');
    const args = ['--require', required, file];
    const { stderr, stdout } = await spawnWithRestarts({ file, args, restarts: 1 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [
      `Completed running ${inspect(file)}`, `Restarting ${inspect(file)}`, `Completed running ${inspect(file)}`, '',
    ].join('\n'));
  });

  it('should not load --import modules in main process', async () => {
    const file = createTmpFile('');
    const imported = fixtures.fileURL('watch-mode/process_exit.js');
    const args = ['--import', imported, file];
    const { stderr, stdout } = await spawnWithRestarts({ file, args, restarts: 1 });

    assert.strictEqual(stderr, '');
    assert.strictEqual(removeGraceMessage(stdout, file), [
      `Completed running ${inspect(file)}`, `Restarting ${inspect(file)}`, `Completed running ${inspect(file)}`, '',
    ].join('\n'));
  });

  describe('inspect', {
    skip: Boolean(process.config.variables.coverage || !process.features.inspector),
  }, () => {
    const silentLogger = { log: () => {}, error: () => {} };
    async function getDebuggedPid(instance, waitForLog = true) {
      const session = await instance.connectInspectorSession();
      await session.send({ method: 'Runtime.enable' });
      if (waitForLog) {
        await session.waitForConsoleOutput('log', 'safe to debug now');
      }
      const { value: innerPid } = (await session.send({
        'method': 'Runtime.evaluate', 'params': { 'expression': 'process.pid' }
      })).result;
      session.disconnect();
      return innerPid;
    }

    it('should start debugger on inner process', async () => {
      const file = fixtures.path('watch-mode/inspect.js');
      const instance = new NodeInstance(['--inspect=0', '--watch'], undefined, file, silentLogger);
      let stderr = '';
      instance.on('stderr', (data) => { stderr += data; });

      const pids = [instance.pid];
      pids.push(await getDebuggedPid(instance));
      instance.resetPort();
      writeFileSync(file, readFileSync(file));
      pids.push(await getDebuggedPid(instance));

      await instance.kill();

      // There should be 3 pids (one parent + 2 restarts).
      // Message about Debugger should only appear twice.
      assert.strictEqual(stderr.match(/Debugger listening on ws:\/\//g).length, 2);
      assert.strictEqual(new Set(pids).size, 3);
    });

    it('should prevent attaching debugger with SIGUSR1 to outer process', { skip: common.isWindows }, async () => {
      const file = fixtures.path('watch-mode/inspect_with_signal.js');
      const instance = new NodeInstance(['--inspect-port=0', '--watch'], undefined, file, silentLogger);
      let stderr = '';
      instance.on('stderr', (data) => { stderr += data; });

      const loggedPid = await new Promise((resolve) => {
        instance.on('stdout', (data) => {
          const matches = data.match(/pid is (\d+)/);
          if (matches) resolve(Number(matches[1]));
        });
      });


      process.kill(instance.pid, 'SIGUSR1');
      process.kill(loggedPid, 'SIGUSR1');
      const debuggedPid = await getDebuggedPid(instance, false);

      await instance.kill();

      // Message about Debugger should only appear once in inner process.
      assert.strictEqual(stderr.match(/Debugger listening on ws:\/\//g).length, 1);
      assert.strictEqual(loggedPid, debuggedPid);
    });
  });
});
