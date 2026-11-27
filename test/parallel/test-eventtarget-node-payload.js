// Flags: --expose-internals
'use strict';
const assert = require('node:assert');
const { NodeEventTarget, Event } = require('internal/event_target');

// Node-style payloads are not Events and must not be inspected for passive state.
{
  const target = new NodeEventTarget();
  const payload = new Proxy({}, {
    get() { throw new Error('Unexpected payload property access'); },
    set() { throw new Error('Unexpected payload property write'); },
  });
  let calls = 0;
  target.once('message', function(value) {
    assert.strictEqual(this, target);
    assert.strictEqual(value, payload);
    calls++;
  });
  target.emit('message', payload);
  target.emit('message', payload);
  assert.strictEqual(calls, 1);
}

// DOM passive cleanup must still precede the next listener in a mixed list.
{
  const target = new NodeEventTarget();
  const payload = {};
  const seen = [];
  target.on('mixed', value => { assert.strictEqual(value, payload); seen.push('node'); });
  target.addEventListener('mixed', event => {
    assert.strictEqual(event.detail, payload);
    event.preventDefault();
    assert.strictEqual(event.defaultPrevented, false);
    seen.push('passive');
  }, { passive: true });
  target.on('mixed', value => { assert.strictEqual(value, payload); seen.push('node2'); });
  target.emit('mixed', payload);
  assert.deepStrictEqual(seen, ['node', 'passive', 'node2']);
}

{
  const target = new NodeEventTarget();
  target.addEventListener('cancel', event => {
    event.preventDefault();
    assert.strictEqual(event.defaultPrevented, false);
  }, { passive: true });
  target.addEventListener('cancel', event => event.preventDefault());
  assert.strictEqual(target.dispatchEvent(new Event('cancel', { cancelable: true })), false);
}

// Both invocation paths keep routing thrown errors and rejected returns.
const expected = [new Error('node throw'), new Error('DOM throw'),
                  new Error('node rejection'), new Error('DOM rejection')];
const seenErrors = [];
function onError(error, origin) {
  assert.strictEqual(origin, 'uncaughtException');
  seenErrors.push(error);
}
process.on('uncaughtException', onError);
const target = new NodeEventTarget();
target.on('node-throw', () => { throw expected[0]; });
target.addEventListener('dom-throw', () => { throw expected[1]; }, { passive: true });
target.on('node-rejection', () => Promise.reject(expected[2]));
target.addEventListener('dom-rejection', () => Promise.reject(expected[3]));
target.emit('node-throw', {});
target.dispatchEvent(new Event('dom-throw'));
target.emit('node-rejection', {});
target.dispatchEvent(new Event('dom-rejection'));
process.once('beforeExit', () => {
  process.removeListener('uncaughtException', onError);
  assert.strictEqual(seenErrors.length, expected.length);
  for (const error of expected) assert.ok(seenErrors.includes(error));
});
