// Flags: --experimental-vm-modules
'use strict';

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

(async () => {
  const queue = vm.createMicrotaskQueue();
  const trace = [];
  const record = (entry) => trace.push(entry);
  const microtaskMode = { type: 'manual', queue };
  const contextA = vm.createContext({ record }, { microtaskMode });
  const contextB = vm.createContext({ record }, { microtaskMode });
  const moduleA = new vm.SourceTextModule(`
    await Promise.resolve();
    record('a');
    Promise.resolve().then(() => record('a-follow-up'));
  `, { context: contextA });
  const moduleB = new vm.SourceTextModule(`
    await Promise.resolve();
    record('b');
    Promise.resolve().then(() => record('b-follow-up'));
  `, { context: contextB });

  await moduleA.link(common.mustNotCall());
  await moduleB.link(common.mustNotCall());

  const evaluationA = moduleA.evaluate();
  const evaluationB = moduleB.evaluate();
  let completed = 0;
  let resolveCompleted;
  const allCompleted = new Promise((resolve) => {
    resolveCompleted = resolve;
  });
  const onCompleted = () => {
    if (++completed === 2) resolveCompleted();
  };
  evaluationA.then(common.mustCall(() => {
    onCompleted();
  }));
  evaluationB.then(common.mustCall(() => {
    onCompleted();
  }));

  assert.deepStrictEqual(trace, []);
  queue.runMicrotasks();
  assert.deepStrictEqual(trace, [
    'a',
    'b',
    'a-follow-up',
    'b-follow-up',
  ]);

  await allCompleted;
  assert.strictEqual(completed, 2);
})().then(common.mustCall());
