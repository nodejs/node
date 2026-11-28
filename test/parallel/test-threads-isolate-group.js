// Flags: --expose-gc
'use strict';

// IsolateGroup, and the isolateGroup option of Worker.

const common = require('../common');
const assert = require('assert');
const { IsolateGroup } = require('node:threads');
const { Worker } = require('node:threads/worker');

const {
  v8_enable_pointer_compression: pointerCompression,
  v8_enable_pointer_compression_shared_cage: sharedCage,
} = process.config.variables;
assert.strictEqual(IsolateGroup.supported,
                   Boolean(pointerCompression) && !sharedCage);

assert(IsolateGroup.current instanceof IsolateGroup);
assert.strictEqual(IsolateGroup.current, IsolateGroup.current);

if (!IsolateGroup.supported) {
  assert.throws(() => new IsolateGroup(),
                { code: 'ERR_ISOLATE_GROUPS_UNSUPPORTED' });
}

for (const isolateGroup of [null, 1, 'group', {}, { __proto__: IsolateGroup.prototype }]) {
  assert.throws(() => new Worker('', { eval: true, isolateGroup }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// Each worker reports that it runs, and then starts a worker of its own in
// its own isolate group.
const code = `
  const { IsolateGroup } = require('node:threads');
  const { Worker, parentPort } = require('node:threads/worker');
  parentPort.postMessage(IsolateGroup.current === IsolateGroup.current);
  const nested = new Worker(
    "require('node:threads/worker').parentPort.postMessage('nested')",
    { eval: true, isolateGroup: IsolateGroup.current });
  nested.on('message', (message) => parentPort.postMessage(message));
`;

function start(isolateGroup) {
  const worker = new Worker(code, { eval: true, isolateGroup });
  const messages = [];
  worker.on('message', (message) => messages.push(message));
  worker.on('exit', common.mustCall((exitCode) => {
    assert.strictEqual(exitCode, 0);
    assert.deepStrictEqual(messages, [true, 'nested']);
  }));
}

start(undefined);
start(IsolateGroup.current);

if (IsolateGroup.supported) {
  // The group stays alive for its workers after its IsolateGroup is
  // collected.
  (() => {
    const group = new IsolateGroup();
    assert.notStrictEqual(group, IsolateGroup.current);
    start(group);
    start(group);
  })();
  globalThis.gc();
}
