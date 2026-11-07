// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { Network, Session } = require('node:inspector');
const session = new Session();
session.connect();
session.post('Network.enable');

const frame = {
  functionName: '函数ü🙂',
  scriptId: '42',
  url: 'file:///路径.js',
  lineNumber: 4,
  columnNumber: 6,
};
const stack = {
  callFrames: [frame, { ...frame, functionName: 'parent' }],
  description: 'async',
  parent: { callFrames: [frame] },
  parentId: { id: '17', debuggerId: '18' },
};
session.once('Network.requestWillBeSent', common.mustCall(({ params }) => {
  assert.deepStrictEqual(params.initiator.stack, stack);
}));
let id = 0;
function emit(stack) {
  Network.requestWillBeSent({
    requestId: `serialization-${id++}`,
    timestamp: 1,
    wallTime: 1,
    initiator: { type: 'script', stack },
    request: { url: 'http://example.org/', method: 'GET', headers: {} },
  });
}
emit(stack);

const cyclic = { callFrames: [], parent: null };
cyclic.parent = cyclic;
const cyclicArray = [];
cyclicArray.push(cyclicArray);
let deep = { callFrames: [] };
for (let i = 0; i < 101; i++) deep = { callFrames: [], parent: deep };
for (const invalid of [
  cyclic,
  { callFrames: [], extra: cyclicArray },
  deep,
  {},
  { callFrames: [], extra: Symbol() },
]) {
  assert.throws(() => emit(invalid), { name: 'TypeError', message: 'Invalid initiator.stack in event' });
}
// A shared object in separate branches is not an ancestor cycle.
emit({ callFrames: [frame, frame], extra: { value: null, flag: true, number: 1.5 } });
assert.throws(() => emit({
  callFrames: [],
  get extra() { throw new Error('getter error'); },
}), { name: 'TypeError', message: 'Invalid initiator.stack in event' });
session.disconnect();
