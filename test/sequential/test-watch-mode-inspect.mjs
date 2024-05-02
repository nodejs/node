import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';
import { writeFileSync, readFileSync } from 'node:fs';
import { setTimeout } from 'node:timers/promises';
import { NodeInstance } from '../common/inspector-helper.js';


if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

common.skipIfInspectorDisabled();

let gettingDebuggedPid = false;

async function getDebuggedPid(instance, waitForLog = true) {
  gettingDebuggedPid = true;
  const session = await instance.connectInspectorSession();
  await session.send({ method: 'Runtime.enable' });
  if (waitForLog) {
    await session.waitForConsoleOutput('log', 'safe to debug now');
  }
  const { value: innerPid } = (await session.send({
    'method': 'Runtime.evaluate', 'params': { 'expression': 'process.pid' }
  })).result;
  session.disconnect();
  gettingDebuggedPid = false;
  return innerPid;
}

function restart(file) {
  writeFileSync(file, readFileSync(file));
  const interval = setInterval(() => {
    if (!gettingDebuggedPid) {
      writeFileSync(file, readFileSync(file));
    }
  }, common.platformTimeout(500));
  return () => clearInterval(interval);
}

describe('watch mode - inspect', () => {
  it('should start debugger on inner process', async () => {
    const file = fixtures.path('watch-mode/inspect.js');
    const instance = new NodeInstance(['--inspect=0', '--watch'], undefined, file);
    let stderr = '';
    const stdout = [];
    instance.on('stderr', (data) => { stderr += data; });
    instance.on('stdout', (data) => { stdout.push(data); });

    const pids = [instance.pid];
    pids.push(await getDebuggedPid(instance));
    instance.resetPort();
    const stopRestarting = restart(file);
    pids.push(await getDebuggedPid(instance));
    stopRestarting();

    await setTimeout(common.platformTimeout(500));
    await instance.kill();

    // There should be a process per restart and one per parent process.
    // Message about Debugger should appear once per restart.
    // On some systems restart can happen multiple times.
    const restarts = stdout.filter((line) => line === 'safe to debug now').length;
    assert.ok(stderr.match(/Debugger listening on ws:\/\//g).length >= restarts);
    assert.ok(new Set(pids).size >= restarts + 1);
  });

  it('should prevent attaching debugger with SIGUSR1 to outer process', { skip: common.isWindows }, async () => {
    const file = fixtures.path('watch-mode/inspect_with_signal.js');
    const instance = new NodeInstance(['--inspect-port=0', '--watch'], undefined, file);
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
