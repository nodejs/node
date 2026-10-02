// Flags: --experimental-logger --expose-gc

'use strict';

// Tests that level methods for disabled levels are resolved to an empty
// function ahead of time for providers whose enablement changes are
// observable, and that they are re-resolved when the enablement changes.

const common = require('../common');
const assert = require('node:assert');
const { channel } = require('node:diagnostics_channel');
const {
  AggregateProvider,
  ConsoleProvider,
  DiagnosticsProvider,
  EventProvider,
  Logger,
  create,
} = require('node:logger');

common.expectWarning(
  'ExperimentalWarning',
  'Logger is an experimental feature and might change at any time',
);

const levelNames = ['trace', 'debug', 'info', 'warn', 'error', 'fatal'];

function isReal(logger, name) {
  return logger[name] === Logger.prototype[name];
}

function assertEnabled(logger, expected) {
  for (const name of levelNames) {
    assert.strictEqual(isReal(logger, name), expected.includes(name),
                       `unexpected method for ${name}`);
  }
}

// Disabled levels share a single empty function and are not own properties.
{
  const logger = create(new EventProvider({ level: 'warn' }));
  assertEnabled(logger, ['warn', 'error', 'fatal']);
  assert.strictEqual(logger.trace, logger.debug);
  assert.strictEqual(logger.trace, logger.info);
  assert.strictEqual(logger.trace.length, 0);
  for (const name of levelNames) {
    assert.strictEqual(Object.hasOwn(logger, name), false);
  }
  assert.ok(logger instanceof Logger);
  assert.strictEqual(logger.constructor, Logger);
}

// Level changes on the provider re-resolve existing loggers and children,
// and an event is delivered only when the method resolves to the real one.
for (const Provider of [ConsoleProvider, EventProvider]) {
  const provider = new Provider({ level: 'info' });
  const logger = create(provider);
  const child = logger.child({ a: 1 });
  assertEnabled(logger, ['info', 'warn', 'error', 'fatal']);
  assertEnabled(child, ['info', 'warn', 'error', 'fatal']);

  provider.level = 'trace';
  assertEnabled(logger, levelNames);
  assertEnabled(child, levelNames);

  provider.level = { name: 'custom', value: 55 };
  assertEnabled(logger, ['fatal']);
  assertEnabled(child, ['fatal']);

  // Loggers created after the change see the new level.
  assertEnabled(create(provider), ['fatal']);
}

{
  const provider = new EventProvider({ level: 'error' });
  const logger = create(provider);
  provider.on('log', common.mustCall(1));
  logger.warn('discarded');
  provider.level = 'warn';
  logger.warn('delivered');
}

// Loggers with different providers do not share resolutions.
{
  const a = create(new EventProvider({ level: 'trace' }));
  const b = create(new EventProvider({ level: 'fatal' }));
  assertEnabled(a, levelNames);
  assertEnabled(b, ['fatal']);
  assert.notStrictEqual(Object.getPrototypeOf(a), Object.getPrototypeOf(b));
  // Loggers with the same provider share the dispatch prototype.
  assert.strictEqual(Object.getPrototypeOf(b),
                     Object.getPrototypeOf(b.child({})));
}

// Aggregate providers enable the union of their members, and are
// re-resolved when a member's level changes.
{
  const first = new EventProvider({ level: 'error' });
  const second = new ConsoleProvider({ level: 'fatal' });
  const aggregate = new AggregateProvider([first, second, first]);
  const logger = create(aggregate);
  const nested = create(new AggregateProvider([aggregate]));
  assertEnabled(logger, ['error', 'fatal']);
  assertEnabled(nested, ['error', 'fatal']);

  second.level = 'debug';
  assertEnabled(logger, ['debug', 'info', 'warn', 'error', 'fatal']);
  assertEnabled(nested, ['debug', 'info', 'warn', 'error', 'fatal']);

  first.level = 'trace';
  assertEnabled(logger, levelNames);
  assertEnabled(nested, levelNames);
}

// Members without isEnabled() are always enabled.
{
  const provider = new EventProvider({ level: 'fatal' });
  const logger = create(new AggregateProvider([provider, { log() {} }]));
  assertEnabled(logger, levelNames);
}

// Collected aggregates do not break member level changes.
{
  const provider = new EventProvider({ level: 'fatal' });
  (() => create(new AggregateProvider([provider])))();
  globalThis.gc();
  setImmediate(common.mustCall(() => {
    provider.level = 'trace';
    assertEnabled(create(provider), levelNames);
  }));
}

// Providers whose enablement cannot be observed keep checking per call.
{
  let enabled = false;
  const provider = {
    isEnabled: common.mustCall(() => enabled, 2),
    log: common.mustCall(),
  };
  const logger = create(provider);
  assertEnabled(logger, levelNames);
  assert.strictEqual(Object.getPrototypeOf(logger), Logger.prototype);
  logger.info('discarded');
  enabled = true;
  logger.info('delivered');
}

{
  const target = channel('test:logger:dispatch');
  const logger = create(new DiagnosticsProvider({
    'test:logger:dispatch': () => true,
  }));
  assertEnabled(logger, levelNames);
  const onEvent = common.mustCall();
  target.subscribe(onEvent);
  logger.info('delivered');
  target.unsubscribe(onEvent);
  logger.info('discarded');
}

// Overriding isEnabled() on a built-in provider opts out of dispatch.
{
  class Custom extends EventProvider {
    isEnabled(level) { return level.value >= 50; }
  }
  const logger = create(new Custom({ level: 'trace' }));
  assertEnabled(logger, levelNames);
  assert.strictEqual(Object.getPrototypeOf(logger), Logger.prototype);

  const aggregate = create(new AggregateProvider([
    new EventProvider({ level: 'fatal' }),
    new Custom(),
  ]));
  assertEnabled(aggregate, levelNames);
}

// Logger subclasses keep their own methods, including overridden levels.
{
  class MyLogger extends Logger {
    debug(message) { return `custom ${message}`; }
    extra() { return 'extra'; }
  }
  const provider = new EventProvider({ level: 'fatal' });
  const logger = new MyLogger(provider);
  assert.ok(logger instanceof MyLogger);
  assert.strictEqual(logger.extra(), 'extra');
  assert.strictEqual(logger.debug('x'), 'custom x');
  assert.notStrictEqual(logger.trace, Logger.prototype.trace);
  assert.strictEqual(logger.trace, create(provider).trace);

  provider.level = 'trace';
  assert.strictEqual(logger.debug('x'), 'custom x');
  assert.strictEqual(logger.trace, Logger.prototype.trace);

  // Plain loggers with the same provider are unaffected by the subclass.
  assertEnabled(create(provider), levelNames);
}

// create(), child(), and `new Logger()` all end up with the same dispatch
// prototype for a given provider, however the provider is passed.
{
  const provider = new EventProvider({ level: 'warn' });
  const fromCreate = create(provider);
  const dispatch = Object.getPrototypeOf(fromCreate);
  assert.notStrictEqual(dispatch, Logger.prototype);
  assert.strictEqual(Object.getPrototypeOf(dispatch), Logger.prototype);
  assert.strictEqual(Object.getPrototypeOf(new Logger(provider)), dispatch);
  assert.strictEqual(Object.getPrototypeOf(fromCreate.child({ a: 1 })),
                     dispatch);
  assert.strictEqual(Object.getPrototypeOf(create(provider, { name: 'x' })),
                     dispatch);
  assert.strictEqual(fromCreate.constructor, Logger);

  // The default provider, whether omitted or replaced by options.
  const defaultDispatch = Object.getPrototypeOf(new Logger());
  assert.notStrictEqual(defaultDispatch, Logger.prototype);
  assert.strictEqual(Object.getPrototypeOf(create()), defaultDispatch);
  assert.strictEqual(Object.getPrototypeOf(create({ name: 'x' })),
                     defaultDispatch);
  assert.strictEqual(Object.getPrototypeOf(create(null, { name: 'x' })),
                     defaultDispatch);
}

// create() reports invalid arguments exactly like the constructor.
{
  for (const args of [
    [{ log() {}, isEnabled: true }],
    [{ notAProvider: true }, {}],
    [new EventProvider(), 'not options'],
    [new EventProvider(), { name: 1 }],
  ]) {
    let expected;
    try {
      new Logger(...args);
    } catch (err) {
      expected = err;
    }
    assert.ok(expected);
    assert.throws(() => create(...args), {
      name: expected.name,
      code: expected.code,
      message: expected.message,
    });
  }
}

// A dispatch prototype is only used for the provider it belongs to, even if
// a logger for another provider is allocated with it.
{
  const quiet = new EventProvider({ level: 'fatal' });
  const loud = new EventProvider({ level: 'trace' });
  const quietDispatch = Object.getPrototypeOf(create(quiet));
  function Crafted() {}
  Crafted.prototype = quietDispatch;
  const logger = Reflect.construct(Logger, [loud], Crafted);
  assertEnabled(logger, levelNames);
  assert.strictEqual(Object.getPrototypeOf(logger),
                     Object.getPrototypeOf(create(loud)));

  // Also when the other provider is not dispatchable.
  const custom = { isEnabled: () => true, log() {} };
  const plain = Reflect.construct(Logger, [custom], Crafted);
  assert.strictEqual(Object.getPrototypeOf(plain), Logger.prototype);
  assertEnabled(plain, levelNames);
}

// Children, grandchildren, and children of subclass instances get the
// provider's plain dispatch prototype, and follow level changes.
{
  class MyLogger extends Logger {}
  const provider = new EventProvider({ level: 'error' });
  const dispatch = Object.getPrototypeOf(create(provider));
  const parents = [create(provider), new Logger(provider),
                   new MyLogger(provider)];
  for (const parent of parents) {
    const child = parent.child({ a: 1 });
    const grandchild = child.child({ b: 2 });
    assert.strictEqual(Object.getPrototypeOf(child), dispatch);
    assert.strictEqual(Object.getPrototypeOf(grandchild), dispatch);
    assert.strictEqual(child instanceof MyLogger, false);
    assertEnabled(grandchild, ['error', 'fatal']);
  }
  provider.level = 'trace';
  assertEnabled(parents[2].child({}).child({}), levelNames);

  // Not dispatchable: children keep the plain prototype.
  const custom = create({ isEnabled: () => true, log() {} });
  assert.strictEqual(Object.getPrototypeOf(custom.child({}).child({})),
                     Logger.prototype);
}

// Function bindings are resolved for children, including when inherited
// from the parent, and a value overriding a parent function is used as is.
{
  const events = [];
  const parent = create({ log(event) { events.push(event); } }, {
    bindings: { dynamic: () => 'resolved', fixed: 1 },
  });
  parent.child({ extra: 2 }).info('inherited');
  parent.child({ dynamic: 'value' }).info('overridden');
  parent.child({}).child({ more: () => 3 }).info('grandchild');
  assert.strictEqual(events[0].bindings.dynamic, 'resolved');
  assert.strictEqual(events[0].bindings.extra, 2);
  assert.strictEqual(events[1].bindings.dynamic, 'value');
  assert.strictEqual(events[2].bindings.dynamic, 'resolved');
  assert.strictEqual(events[2].bindings.more, 3);
}

// Children defer freezing their bindings, but providers and users only ever
// observe them frozen.
{
  const provider = {
    isEnabled: common.mustCall((level, context) => {
      assert.ok(Object.isFrozen(context.bindings));
      return true;
    }, 2),
    log: common.mustCall((event, context) => {
      assert.ok(Object.isFrozen(event.bindings));
      assert.ok(Object.isFrozen(context.bindings));
    }, 2),
  };
  const parent = create(provider, { bindings: { a: 1 } });
  const child = parent.child({ b: 2 });
  assert.ok(Object.isFrozen(parent.child({ c: 3 }).bindings));
  child.child({ d: 4 }).info('grandchild first');
  child.info('child');
}
