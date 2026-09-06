// Flags: --experimental-logger --no-warnings

'use strict';

// ConsoleProvider serializes events created by Logger without going through
// the configured serializer when it is the default one. Check that the output
// is identical to the default serializer's output for the same record.

require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const {
  AggregateProvider,
  ConsoleProvider,
  create,
  levels,
} = require('node:logger');

function capture(provider) {
  const lines = [];
  provider.stream.write = (data) => {
    lines.push(data);
    return true;
  };
  return lines;
}

const als = new AsyncLocalStorage();

const err = new Error('boom');
err.code = 'E_BOOM';

function makeError(message, props) {
  return Object.assign(new Error(message), props);
}

const selfCause = new Error('self');
selfCause.cause = selfCause;

let deepCause = new Error('bottom');
for (let i = 0; i < 12; i++) deepCause = new Error(`level ${i}`, { cause: deepCause });

const enumerableMessage = new Error('hidden');
Object.defineProperty(enumerableMessage, 'message', {
  value: 'visible "message"', enumerable: true,
});

const noStack = new Error('no stack');
delete noStack.stack;

class CustomError extends Error {
  get name() { return 'CustomError'; }
}

const errorVariants = [
  err,
  new TypeError('type \n error'),
  new Error('with cause', { cause: new Error('inner', { cause: 'root' }) }),
  new Error('string cause', { cause: 'just a string' }),
  selfCause,
  deepCause,
  enumerableMessage,
  makeError('props', { status: 500, retry: true, extra: undefined }),
  makeError('nested', { inner: new RangeError('nested error') }),
  makeError('object prop', { details: { a: 1 } }),
  makeError('index key', { 0: 'zero' }),
  makeError('code', { code: 42 }),
  makeError('toJSON', { toJSON() { return 'custom'; } }),
  new AggregateError([new Error('a')], 'aggregate'),
  noStack,
  new CustomError('custom'),
];

const bindingVariants = [
  undefined,
  { service: 'api', port: 3000, on: true, none: null, skip: undefined },
  { request: () => als.getStore(), service: 'api' },
  { 1: 'index', service: 'api' },
  { level: 'binding level' },
  { pid: 'binding pid' },
  { nested: { a: 1 } },
  { big: 1n },
  { err },
];

const attributeVariants = [
  undefined,
  {},
  { i: 1, user: 'x' },
  { text: 'quote " backslash \\ newline \n tab \t \u2028 \ud800 \u{1F600}' },
  { nan: NaN, inf: -Infinity, negZero: -0, big: 1.5e300, small: 5e-324 },
  { yes: true, no: false, nothing: null, missing: undefined },
  { fn: () => 'resolved', fnObject: () => ({ a: 1 }) },
  { sym: Symbol('value'), [Symbol('key')]: 'symbol key' },
  { 2: 'index', a: 1 },
  { service: 'attribute service' },
  { level: 1, levelName: 2, message: 3, name: 4, pid: 5, timestamp: 6 },
  { nested: { a: [1, 2, { b: 'c' }] } },
  { err },
  { big: 10n },
  { date: new Date(0) },
  { list: [1, 'two', null] },
  { '': 'empty key', 'k"ey': 'quoted key' },
  { long: 'x'.repeat(100), longEscaped: `${'y'.repeat(100)}"` },
  { emoji: '\u{1F600}', [`key\n${'z'.repeat(70)}`]: 1 },
  ...errorVariants.map((error) => ({ error, after: 1 })),
];

const messages = [
  'hello',
  '',
  undefined,
  42,
  null,
  true,
  { object: 'message' },
  Symbol('message'),
  () => {},
  ...errorVariants,
];

// Child bindings, whose serialization extends the parent's when possible.
const childBindingVariants = [
  {},
  { child: true },
  { service: 'override' },
  { service: 'api' },
  { 3: 'index' },
  { level: 'child level' },
  { skip: undefined, after: 'x' },
  { nested: { a: 1 } },
  { childFn: () => 'child fn' },
];

let compared = 0;
for (const flatten of [false, true]) {
  for (const pid of [false, true]) {
    const fast = new ConsoleProvider({ flatten, pid, level: 'trace' });
    const generic = new ConsoleProvider({
      flatten,
      pid,
      level: 'trace',
      // Any serializer other than the default one disables the fast path.
      serializer: (record) => fast.serializer(record),
    });
    const fastLines = capture(fast);
    const genericLines = capture(generic);
    const aggregate = new AggregateProvider([fast, generic]);
    // Forwards events without the context.
    const wrapper = { log(event) { aggregate.log(event); } };

    for (const bindings of bindingVariants) {
      for (const [name, provider] of [[undefined, aggregate],
                                      ['logger "name"', aggregate],
                                      ['wrapped', wrapper]]) {
        const logger = create(provider, { name, bindings });
        als.run({ requestId: 1 }, () => {
          for (const attributes of attributeVariants) {
            for (const message of messages) {
              if (attributes === undefined) {
                logger.info(message);
              } else {
                logger.info(message, attributes);
              }
            }
          }
          logger.log({ name: 'custom "level"', value: 35 }, 'custom');
          for (const childBindings of childBindingVariants) {
            const child = logger.child(childBindings);
            const grandchild = child.child({ grandchild: 1 });
            // The grandchild logs first, before its parent ever has.
            for (let i = 0; i < 2; i++) {
              grandchild.warn('from grandchild', { i });
              child.warn('from child', { i });
            }
          }
        });
      }
    }

    // Objects passed to the provider directly are not Logger events.
    aggregate.log({ level: levels.info, message: 'direct', extra: 1 });

    assert.strictEqual(fastLines.length, genericLines.length);
    for (let i = 0; i < fastLines.length; i++) {
      assert.strictEqual(fastLines[i], genericLines[i],
                         `flatten=${flatten} pid=${pid} line ${i}`);
    }
    compared += fastLines.length;
  }
}
assert.ok(compared > 0);
