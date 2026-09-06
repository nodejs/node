// Flags: --experimental-logger

'use strict';

const common = require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const EventEmitter = require('node:events');
const { isBuiltin } = require('node:module');
const loggerModule = require('node:logger');
const {
  AggregateProvider,
  ConsoleProvider,
  EventProvider,
  Logger,
  create,
  getDefaultProvider,
  levels,
  setDefaultProvider,
} = loggerModule;

common.expectWarning(
  'ExperimentalWarning',
  'Logger is an experimental feature and might change at any time',
);

assert.strictEqual(isBuiltin('node:logger'), true);
assert.strictEqual(isBuiltin('logger'), false);
assert.strictEqual(loggerModule, create);

import('node:logger').then(common.mustCall((logger) => {
  assert.strictEqual(logger.default, create);
  assert.strictEqual(logger.create, create);
  assert.strictEqual(logger.getDefaultProvider, getDefaultProvider);
  assert.strictEqual(logger.setDefaultProvider, setDefaultProvider);
}));

{
  const defaultProvider = getDefaultProvider();
  const first = loggerModule();
  const second = create();

  assert.strictEqual(Object.hasOwn(create, 'defaultProvider'), false);
  assert.strictEqual(getDefaultProvider(), defaultProvider);
  assert.ok(defaultProvider instanceof ConsoleProvider);
  assert.ok(first instanceof Logger);
  assert.strictEqual(first.provider, defaultProvider);
  assert.strictEqual(second.provider, defaultProvider);
}

{
  const original = getDefaultProvider();
  const provider = new EventProvider();
  const reentrantProvider = new EventProvider();
  const events = [];
  const changes = [];
  const eventName = 'defaultLoggerProviderChanged';
  const listener = common.mustCall((changedProvider, currentProvider) => {
    changes.push([changedProvider, currentProvider]);
    assert.throws(
      () => setDefaultProvider(reentrantProvider),
      { code: 'ERR_INVALID_STATE' },
    );
  }, 2);
  provider.on('log', common.mustCall((event) => events.push(event)));

  process.on(eventName, listener);
  try {
    setDefaultProvider(provider);
    assert.deepStrictEqual(changes, [[provider, original]]);
    setDefaultProvider(provider);
    assert.deepStrictEqual(changes, [[provider, original]]);
    assert.throws(
      () => setDefaultProvider({}),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
    assert.deepStrictEqual(changes, [[provider, original]]);

    const logger = create({ name: 'custom-default' });
    assert.strictEqual(getDefaultProvider(), provider);
    assert.strictEqual(logger.provider, provider);
    assert.strictEqual(new Logger().provider, provider);
    logger.info('redirected');
    assert.strictEqual(events[0].message, 'redirected');
  } finally {
    setDefaultProvider(original);
    process.off(eventName, listener);
  }

  assert.deepStrictEqual(changes, [
    [provider, original],
    [original, provider],
  ]);
  assert.strictEqual(getDefaultProvider(), original);
}

{
  const original = getDefaultProvider();
  const provider = new EventProvider();
  const expected = new Error('default provider rejected');
  const eventName = 'defaultLoggerProviderChanged';
  const listener = common.mustCall(() => { throw expected; });

  process.once(eventName, listener);
  try {
    assert.throws(
      () => setDefaultProvider(provider),
      (error) => error === expected,
    );
    assert.strictEqual(getDefaultProvider(), original);

    setDefaultProvider(provider);
    assert.strictEqual(getDefaultProvider(), provider);
  } finally {
    process.off(eventName, listener);
    if (getDefaultProvider() !== original) {
      setDefaultProvider(original);
    }
  }
}

{
  const invalidThis = {
    code: 'ERR_INVALID_THIS',
    message: 'Value of "this" must be of type Logger',
    name: 'TypeError',
  };
  const receivers = [null, undefined, { __proto__: Logger.prototype }];
  const getters = ['bindings', 'name', 'provider'];
  const methods = [
    'child',
    'debug',
    'error',
    'fatal',
    'info',
    'isEnabled',
    'log',
    'trace',
    'warn',
  ];

  for (const receiver of receivers) {
    for (const name of getters) {
      const getter = Object.getOwnPropertyDescriptor(Logger.prototype, name).get;
      assert.throws(() => Reflect.apply(getter, receiver, []), invalidThis);
    }
    for (const name of methods) {
      assert.throws(
        () => Reflect.apply(Logger.prototype[name], receiver, []),
        invalidThis,
      );
    }
  }
}

{
  const events = [];
  const contexts = [];
  const provider = {
    isEnabled(level, context) {
      contexts.push(context);
      return level.value >= levels.debug.value;
    },
    log: common.mustCall(function log(event) {
      assert.strictEqual(this, provider);
      events.push(event);
    }, 2),
  };
  const inputBindings = { service: 'api' };
  const logger = create(provider, {
    name: 'root',
    bindings: inputBindings,
  });

  inputBindings.service = 'changed';

  assert.strictEqual(logger.provider, provider);
  assert.strictEqual(logger.name, 'root');
  assert.deepStrictEqual(logger.bindings, {
    __proto__: null,
    service: 'api',
  });
  assert.strictEqual(Object.isFrozen(logger.bindings), true);
  assert.strictEqual(logger.isEnabled('trace'), false);
  assert.strictEqual(logger.isEnabled('debug'), true);

  logger.trace('filtered');

  const inputAttributes = { requestId: 1 };
  logger.info('request received', inputAttributes);
  inputAttributes.requestId = 2;

  assert.strictEqual(events.length, 1);
  const event = events[0];
  assert.strictEqual(event.message, 'request received');
  assert.strictEqual(event.name, 'root');
  assert.strictEqual(event.level, levels.info);
  assert.strictEqual(typeof event.timestamp, 'number');
  assert.deepStrictEqual(event.bindings, {
    __proto__: null,
    service: 'api',
  });
  assert.deepStrictEqual(event.attributes, {
    __proto__: null,
    requestId: 1,
  });
  assert.strictEqual(Object.isFrozen(event), true);
  assert.strictEqual(Object.isFrozen(event.attributes), true);
  assert.strictEqual(contexts.at(-1).name, 'root');

  const child = logger.child({ service: 'worker', workerId: 2 });
  assert.strictEqual(child.provider, provider);
  assert.strictEqual(child.name, 'root');
  assert.deepStrictEqual(child.bindings, {
    __proto__: null,
    service: 'worker',
    workerId: 2,
  });

  child.log({ name: 'notice', value: 35 }, 'custom level');
  assert.strictEqual(events.length, 2);
  assert.deepStrictEqual(events[1].level, {
    __proto__: null,
    name: 'notice',
    value: 35,
  });
}

{
  const events = [];
  const provider = {
    log(event) {
      events.push(event);
    },
  };
  const logger = new Logger(provider);

  logger.fatal('fatal message');
  assert.strictEqual(events.length, 1);
  assert.strictEqual(logger.isEnabled('trace'), true);
}

{
  const bindingStorage = new AsyncLocalStorage();
  const attributeStorage = new AsyncLocalStorage();
  const request = common.mustCall(() => bindingStorage.getStore(), 3);
  const operation = common.mustCall(() => attributeStorage.getStore(), 2);
  const events = [];
  const logger = create({
    log(event) {
      events.push(event);
    },
  }, {
    bindings: { rawStorage: bindingStorage, request },
  });

  logger.info('outside', { operation, rawStorage: attributeStorage });
  bindingStorage.run({ requestId: 1 }, () => {
    attributeStorage.run('create', () => {
      logger.info('inside', { operation });
    });
  });
  bindingStorage.run({ requestId: 2 }, () => {
    logger.info('second request');
  });

  assert.strictEqual(logger.bindings.request, request);
  assert.strictEqual(events[0].bindings.rawStorage, bindingStorage);
  assert.strictEqual(events[0].bindings.request, undefined);
  assert.strictEqual(events[0].attributes.rawStorage, attributeStorage);
  assert.strictEqual(events[0].attributes.operation, undefined);
  assert.deepStrictEqual(events[1].bindings.request, { requestId: 1 });
  assert.strictEqual(events[1].attributes.operation, 'create');
  assert.deepStrictEqual(events[2].bindings.request, { requestId: 2 });
}

{
  const logger = create({ name: 'options-only' });
  assert.strictEqual(logger.name, 'options-only');
  assert.ok(logger.provider instanceof ConsoleProvider);
}

{
  const provider = new EventProvider({ level: 'debug' });
  const logger = create(provider, { name: 'events' });
  const events = [];
  const warnings = [];

  assert.ok(provider instanceof EventEmitter);
  assert.strictEqual(provider.level, levels.debug);

  provider.on('log', common.mustCall((event) => {
    events.push(event);
  }, 3));
  provider.on('warn', common.mustCall((event) => {
    warnings.push(event);
  }));

  const message = { text: 'debug message' };
  logger.trace('filtered');
  logger.debug(message);
  logger.warn('warn message');
  logger.log({ name: 'notice', value: levels.warn.value }, 'notice message');
  logger.error('error message');

  assert.strictEqual(events.length, 3);
  assert.strictEqual(events[0].level, levels.debug);
  assert.strictEqual(events[0].message, message);
  assert.strictEqual(events[1].level.name, 'notice');
  assert.strictEqual(events[1].level.value, levels.warn.value);
  assert.strictEqual(events[2].level, levels.error);
  assert.strictEqual(warnings.length, 1);
  assert.strictEqual(warnings[0].level, levels.warn);

  provider.level = 'fatal';
  assert.strictEqual(provider.level, levels.fatal);
  assert.strictEqual(logger.isEnabled('error'), false);

  const customLevel = { name: 'notice', value: 35 };
  provider.level = customLevel;
  customLevel.value = 0;
  assert.strictEqual(provider.level.name, 'notice');
  assert.strictEqual(provider.level.value, 35);
  assert.strictEqual(Object.isFrozen(provider.level), true);
}

{
  const calls = [];
  let deliveredEvent;
  let loggerContext;
  const disabled = {
    isEnabled: common.mustCall(function isEnabled(level, context) {
      assert.strictEqual(level, levels.info);
      loggerContext ??= context;
      assert.strictEqual(context, loggerContext);
      return false;
    }, 2),
    log: common.mustNotCall(),
  };
  const first = {
    isEnabled: common.mustCall(function isEnabled(level, context) {
      assert.strictEqual(level, levels.info);
      loggerContext ??= context;
      assert.strictEqual(context, loggerContext);
      return true;
    }, 2),
    log: common.mustCall(function log(event, context) {
      assert.strictEqual(this, first);
      assert.strictEqual(context, loggerContext);
      deliveredEvent = event;
      calls.push('first');
    }),
  };
  const second = {
    log: common.mustCall(function log(event, context) {
      assert.strictEqual(this, second);
      assert.strictEqual(context, loggerContext);
      assert.strictEqual(event, deliveredEvent);
      calls.push('second');
    }),
  };
  const input = [disabled, first, second];
  const provider = new AggregateProvider(input);
  const logger = create(provider, {
    name: 'aggregate',
    bindings: { service: 'api' },
  });

  input.length = 0;
  assert.strictEqual(Object.isFrozen(provider.providers), true);
  assert.deepStrictEqual(provider.providers, [disabled, first, second]);

  logger.info('fan out');

  assert.deepStrictEqual(calls, ['first', 'second']);
  assert.strictEqual(loggerContext.name, 'aggregate');
  assert.deepStrictEqual(loggerContext.bindings, {
    __proto__: null,
    service: 'api',
  });
}

{
  const provider = new AggregateProvider([]);
  const logger = create(provider);

  assert.strictEqual(logger.isEnabled('fatal'), false);
  logger.fatal('discarded');
}

{
  const expected = new Error('provider failure');
  const provider = new AggregateProvider([
    { log() { throw expected; } },
    { log: common.mustNotCall() },
  ]);
  const logger = create(provider);

  assert.throws(() => logger.info('failed'), (error) => error === expected);
}

{
  const destination = { log: common.mustCall() };
  const provider = new AggregateProvider([
    new AggregateProvider([destination]),
  ]);

  create(provider).info('nested');
}

assert.throws(
  () => create(1),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => create({ log: 'not a function' }, {}),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => create({ log() {}, isEnabled: true }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => create({ bindings: [] }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => new AggregateProvider(),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => new AggregateProvider([1]),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => new AggregateProvider([{ log() {}, isEnabled: true }]),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

{
  const logger = create({ log() {} });

  assert.throws(
    () => logger.info('message', []),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => logger.log('unknown', 'message'),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  assert.throws(
    () => logger.log({ name: 'custom', value: 1.5 }, 'message'),
    { code: 'ERR_OUT_OF_RANGE' },
  );
}

assert.strictEqual(Object.isFrozen(levels), true);
assert.strictEqual(Object.isFrozen(levels.info), true);
