'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const queue = vm.createMicrotaskQueue();
const fake = { __proto__: Object.getPrototypeOf(queue) };
const proxy = new Proxy(queue, {});

const script = new vm.Script('');
const createContext = (options) => vm.createContext({}, options);
const runInNewContext = (options) => vm.runInNewContext('', {}, options);
const scriptRunInNewContext =
  (options) => script.runInNewContext({}, options);
const contextCreators = [
  createContext,
  runInNewContext,
  scriptRunInNewContext,
];

for (const create of contextCreators) {
  assert.throws(() => {
    create({ microtaskMode: { type: 'automatic', queue } });
  }, { code: 'ERR_INVALID_ARG_VALUE' });

  for (const invalid of [fake, proxy, {}, null, undefined, 1, 'queue']) {
    const options = { microtaskMode: { type: 'manual', queue: invalid } };
    assert.throws(() => create(options), { code: 'ERR_INVALID_ARG_TYPE' });
  }
}

// A revoked Proxy is also rejected, but not with `ERR_INVALID_ARG_TYPE`:
// building that error's message reads `value.constructor` (see
// `determineSpecificType()` in lib/internal/errors.js), which throws on a
// revoked Proxy before the intended error can be constructed. This is a
// pre-existing, feature-independent limitation of `ERR_INVALID_ARG_TYPE`
// itself (e.g. `fs.readFileSync()` hits the same thing), not something
// specific to `vm.MicrotaskQueue` validation. What this test guards is that
// an invalid queue is still rejected, not accepted and used.
{
  const { proxy: revoked, revoke } = Proxy.revocable(queue, {});
  revoke();
  const options = { microtaskMode: { type: 'manual', queue: revoked } };
  assert.throws(() => vm.createContext({}, options), TypeError);
}

// Changing a real handle's JS prototype must not change its native identity.
const runMicrotasks = queue.runMicrotasks;
Object.setPrototypeOf(queue, null);
const trace = [];
const context = vm.createContext({ trace }, {
  microtaskMode: { type: 'manual', queue },
});
vm.runInContext('Promise.resolve().then(() => trace.push(1))', context);
assert.deepStrictEqual(trace, []);
runMicrotasks.call(queue);
assert.deepStrictEqual(trace, [1]);

// Draining one queue must leave other queues and Node's own work pending.
const first = vm.createMicrotaskQueue();
const second = vm.createMicrotaskQueue();
const events = [];
process.nextTick(common.mustCall(() => events.push('tick')));
Promise.resolve().then(() => events.push('main')).then(common.mustCall());
for (const [handle, label] of [[first, 'first'], [second, 'second']]) {
  vm.runInNewContext('Promise.resolve().then(() => events.push(label))',
                     { events, label },
                     { microtaskMode: { type: 'manual', queue: handle } });
}
first.runMicrotasks();
assert.deepStrictEqual(events, ['first']);
second.runMicrotasks();
assert.deepStrictEqual(events, ['first', 'second']);
