// Flags: --expose-internals
'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { EventEmitter } = require('events');
const { PassThrough } = require('stream');
const createRepl = require('internal/debugger/inspect_repl');

function createGate() {
  let resolve;
  const promise = new Promise((res) => {
    resolve = res;
  });
  return { promise, resolve };
}

function createAgent(domain, calls, gates) {
  const agent = new EventEmitter();
  const method = (name) => async () => {
    calls.push(`${domain}.${name}`);
    if (domain === 'Runtime' && name === 'runIfWaitingForDebugger') {
      const gate = gates.shift();
      if (gate) {
        await gate.promise;
      }
    }
  };

  agent.enable = method('enable');
  agent.setSamplingInterval = method('setSamplingInterval');
  agent.setAsyncCallStackDepth = method('setAsyncCallStackDepth');
  agent.setBlackboxPatterns = method('setBlackboxPatterns');
  agent.setPauseOnExceptions = method('setPauseOnExceptions');
  agent.runIfWaitingForDebugger = method('runIfWaitingForDebugger');
  return agent;
}

function evalCommand(repl, command) {
  return new Promise((resolve, reject) => {
    repl.eval(
      command,
      repl.context,
      'debugger-repl-test',
      (error, result) => {
        if (error) {
          reject(error);
        } else {
          resolve(result);
        }
      },
    );
  });
}

async function assertCommandWaitsForInit(
  repl, command, initGate, pauseGate, renderGate, calls) {
  let settled = false;
  const promise = evalCommand(repl, command).then(() => {
    settled = true;
  });

  await new Promise(setImmediate);
  assert.strictEqual(
    settled,
    false,
    `${command} resolved before post-connect initialization completed: ${calls}`,
  );

  initGate.resolve();
  await new Promise(setImmediate);
  assert.strictEqual(
    settled,
    false,
    `${command} resolved before the initial break was rendered: ${calls}`,
  );

  pauseGate.resolve();
  await new Promise(setImmediate);
  assert.strictEqual(
    settled,
    false,
    `${command} resolved before the initial break render completed: ${calls}`,
  );

  renderGate.resolve();
  await promise;
  assert.strictEqual(settled, true);
}

(async () => {
  const calls = [];
  const runGate = createGate();
  const restartGate = createGate();
  const gates = [null, runGate, restartGate];
  const initialRenderGate = createGate();
  const runRenderGate = createGate();
  const restartRenderGate = createGate();
  const renderGates = [initialRenderGate, runRenderGate, restartRenderGate];
  const inspector = {
    client: new EventEmitter(),
    domainNames: ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'],
    options: { script: 'fixture.js' },
    stdin: new PassThrough(),
    stdout: new PassThrough(),
    run: common.mustCall(async () => {
      calls.push('inspector.run');
    }, 2),
    print(text, addNewline = true) {
      this.stdout.write(`${text}${addNewline ? '\n' : ''}`);
    },
    suspendReplWhile(fn) {
      return fn();
    },
  };

  const pausedEvent = {
    callFrames: [{
      functionName: '',
      location: { scriptId: '1', lineNumber: 0, columnNumber: 0 },
      scopeChain: [],
    }],
    reason: 'other',
  };
  for (const domain of inspector.domainNames) {
    inspector[domain] = createAgent(domain, calls, gates);
  }
  inspector.Debugger.getScriptSource = async () => {
    await renderGates.shift().promise;
    return { scriptSource: 'const value = 1;\n' };
  };
  const emitPause = common.mustCall(() => {
    inspector.Debugger.emit('paused', pausedEvent);
  }, 3);
  const initialPauseGate = { resolve: emitPause };
  const runPauseGate = { resolve: emitPause };
  const restartPauseGate = { resolve: emitPause };

  let replSettled = false;
  const replPromise = createRepl(inspector)().then((repl) => {
    replSettled = true;
    return repl;
  });
  await new Promise(setImmediate);
  assert.strictEqual(replSettled, false);
  initialPauseGate.resolve();
  await new Promise(setImmediate);
  assert.strictEqual(replSettled, false);
  initialRenderGate.resolve();
  const repl = await replPromise;

  await assertCommandWaitsForInit(
    repl, 'run', runGate, runPauseGate, runRenderGate, calls);
  await assertCommandWaitsForInit(
    repl, 'restart', restartGate, restartPauseGate,
    restartRenderGate, calls);

  assert.deepStrictEqual(
    calls.filter((call) => (
      call === 'inspector.run' ||
      call === 'Runtime.runIfWaitingForDebugger'
    )),
    [
      'Runtime.runIfWaitingForDebugger',
      'inspector.run',
      'Runtime.runIfWaitingForDebugger',
      'inspector.run',
      'Runtime.runIfWaitingForDebugger',
    ],
  );

  repl.close();
})().then(common.mustCall());
