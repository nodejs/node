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
  agent.getScriptSource = async () => {
    calls.push(`${domain}.getScriptSource`);
    return { scriptSource: 'let x = 1;\nx = x + 1;\n' };
  };
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

function createChild() {
  const child = new EventEmitter();
  child.exitCode = null;
  child.signalCode = null;
  return child;
}

function emitInitialPause(inspector) {
  inspector.Debugger.emit('paused', {
    reason: 'Break on start',
    callFrames: [{
      callFrameId: 'call-frame-id',
      functionName: '',
      location: {
        scriptId: '1',
        lineNumber: 0,
        columnNumber: 0,
      },
      scopeChain: [],
    }],
  });
}

async function assertCommandWaitsForInit(repl, command, gate, calls) {
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

  gate.resolve();
  await promise;
  assert.strictEqual(settled, true);
}

async function assertStartupWaitsForInitialPauseRender() {
  const calls = [];
  const runtimeGate = createGate();
  const pauseRenderGate = createGate();
  const gates = [runtimeGate];
  const inspector = {
    child: createChild(),
    client: new EventEmitter(),
    domainNames: ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'],
    options: { script: 'script.js' },
    stdin: new PassThrough(),
    stdout: new PassThrough(),
    print(text, addNewline = true) {
      inspector.stdout.write(addNewline ? `${text}\n` : text);
    },
    suspendReplWhile(fn) {
      return Promise.resolve(fn()).then(() => pauseRenderGate.promise);
    },
  };

  for (const domain of inspector.domainNames) {
    inspector[domain] = createAgent(domain, calls, gates);
  }

  let repl;
  let settled = false;
  const promise = createRepl(inspector)().then((createdRepl) => {
    repl = createdRepl;
    settled = true;
  });

  runtimeGate.resolve();
  await new Promise(setImmediate);
  assert.strictEqual(
    settled,
    false,
    `startup resolved before initial pause was observed: ${calls}`,
  );

  emitInitialPause(inspector);
  await new Promise(setImmediate);
  assert.strictEqual(
    settled,
    false,
    `startup resolved before initial pause render completed: ${calls}`,
  );

  pauseRenderGate.resolve();
  await promise;
  assert.strictEqual(settled, true);
  repl.close();
}

async function assertRunAndRestartWaitForInit() {
  const calls = [];
  const runGate = createGate();
  const restartGate = createGate();
  const gates = [null, runGate, restartGate];
  const inspector = {
    child: null,
    client: new EventEmitter(),
    domainNames: ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'],
    options: {},
    stdin: new PassThrough(),
    stdout: new PassThrough(),
    run: common.mustCall(async () => {
      calls.push('inspector.run');
      inspector.child = createChild();
    }, 2),
    suspendReplWhile(fn) {
      return fn();
    },
  };

  for (const domain of inspector.domainNames) {
    inspector[domain] = createAgent(domain, calls, gates);
  }

  const repl = await createRepl(inspector)();
  inspector.options = { script: 'script.js' };

  await assertCommandWaitsForInit(repl, 'run', runGate, calls);
  await assertCommandWaitsForInit(repl, 'restart', restartGate, calls);

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
}

(async () => {
  await assertRunAndRestartWaitForInit();
  await assertStartupWaitsForInitialPauseRender();
})().then(common.mustCall());
