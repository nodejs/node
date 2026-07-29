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

(async () => {
  const calls = [];
  const runGate = createGate();
  const restartGate = createGate();
  const gates = [null, runGate, restartGate];
  const inspector = {
    client: new EventEmitter(),
    domainNames: ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'],
    stdin: new PassThrough(),
    stdout: new PassThrough(),
    run: common.mustCall(async () => {
      calls.push('inspector.run');
    }, 2),
    suspendReplWhile(fn) {
      return fn();
    },
  };

  for (const domain of inspector.domainNames) {
    inspector[domain] = createAgent(domain, calls, gates);
  }

  const repl = await createRepl(inspector)();

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
})().then(common.mustCall());
