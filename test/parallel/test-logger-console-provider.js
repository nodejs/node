// Flags: --experimental-logger

'use strict';

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { ConsoleProvider, levels } = require('node:logger');
const { inspect } = require('node:util');

function run(destination, level, statement, options = {}) {
  const script = `
    const { ConsoleProvider, create } = require('node:logger');
    const provider = new ConsoleProvider({
      destination: ${JSON.stringify(destination)},
      level: ${JSON.stringify(level)},
      sync: true,
      ...${JSON.stringify(options)},
    });
    const logger = create(provider, {
      name: 'example',
      bindings: { service: 'api' },
    });
    ${statement}
    provider.flushSync();
  `;

  return spawnSync(process.execPath, [
    '--experimental-logger',
    '--no-warnings',
    '-e',
    script,
  ], {
    encoding: 'utf8',
  });
}

{
  const result = run(
    'stdout',
    'debug',
    "logger.trace('filtered'); logger.info('started', { port: 3000 });",
  );

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const event = JSON.parse(result.stdout);
  assert.strictEqual(event.message, 'started');
  assert.strictEqual(event.name, 'example');
  assert.deepStrictEqual(event.level, { name: 'info', value: 30 });
  assert.deepStrictEqual(event.bindings, { service: 'api' });
  assert.deepStrictEqual(event.attributes, { port: 3000 });
  assert.strictEqual(typeof event.timestamp, 'number');
  assert.strictEqual('pid' in event, false);
}

{
  const result = run(
    'stdout',
    'info',
    `
      logger.info('flattened', {
        level: 'attribute level',
        levelName: 'attribute name',
        message: 'attribute message',
        name: 'attribute name',
        pid: -1,
        requestId: 1,
        service: 'worker',
        timestamp: 0,
      });
    `,
    { flatten: true, pid: true },
  );

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const event = JSON.parse(result.stdout);
  assert.strictEqual('attributes' in event, false);
  assert.strictEqual('bindings' in event, false);
  assert.strictEqual(event.level, 30);
  assert.strictEqual(event.levelName, 'info');
  assert.strictEqual(event.message, 'flattened');
  assert.strictEqual(event.name, 'example');
  assert.ok(event.pid > 0);
  assert.strictEqual(event.requestId, 1);
  assert.strictEqual(event.service, 'worker');
  assert.ok(event.timestamp > 0);
}

{
  const script = `
    const { ConsoleProvider, create } = require('node:logger');
    const provider = new ConsoleProvider({ pid: true, sync: true });
    const logger = create(provider);
    logger.info('with pid');
    provider.flushSync();
  `;
  const result = spawnSync(process.execPath, [
    '--experimental-logger',
    '--no-warnings',
    '-e',
    script,
  ], { encoding: 'utf8' });

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const event = JSON.parse(result.stdout);
  assert.strictEqual(event.message, 'with pid');
  assert.ok(Number.isInteger(event.pid));
  assert.ok(event.pid > 0);
}

{
  const result = run(
    'stderr',
    'error',
    "logger.warn('filtered'); logger.error('failed');",
  );

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stdout, '');
  const event = JSON.parse(result.stderr);
  assert.strictEqual(event.message, 'failed');
  assert.deepStrictEqual(event.level, { name: 'error', value: 50 });
}

{
  const result = run(
    'stdout',
    'trace',
    `
      const shared = { value: 1 };
      const message = {
        bigint: 9007199254740993n,
        error: Object.assign(new Error('boom'), { code: 'ERR_TEST' }),
        first: shared,
        second: shared,
      };
      message.self = message;
      logger.info(message);
    `,
  );

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const event = JSON.parse(result.stdout);
  assert.strictEqual(event.message.bigint, '9007199254740993');
  assert.strictEqual(event.message.error.name, 'Error');
  assert.strictEqual(event.message.error.message, 'boom');
  assert.match(event.message.error.stack, /^Error: boom/);
  assert.strictEqual(event.message.error.code, 'ERR_TEST');
  assert.deepStrictEqual(event.message.first, { value: 1 });
  assert.deepStrictEqual(event.message.second, { value: 1 });
  assert.strictEqual(event.message.self, '[Circular]');
}

{
  const script = `
    const { ConsoleProvider, create } = require('node:logger');
    const { inspect } = require('node:util');
    const provider = new ConsoleProvider({ serializer: inspect, sync: true });
    const logger = create(provider);
    const cyclic = { value: 1n };
    cyclic.self = cyclic;
    logger.info('inspected', { cyclic });
    provider.flushSync();
  `;
  const result = spawnSync(process.execPath, [
    '--experimental-logger',
    '--no-warnings',
    '-e',
    script,
  ], { encoding: 'utf8' });

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /message: 'inspected'/);
  assert.match(result.stdout, /value: 1n/);
  assert.match(result.stdout, /\[Circular \*\d+\]/);
}

{
  const provider = new ConsoleProvider({ flatten: true, serializer: inspect });
  assert.strictEqual(provider.flatten, true);
  assert.strictEqual(provider.serializer, inspect);

  provider.level = 'warn';
  assert.strictEqual(provider.level, levels.warn);

  const customLevel = { name: 'notice', value: 35 };
  provider.level = customLevel;
  customLevel.value = 0;
  assert.strictEqual(provider.level.name, 'notice');
  assert.strictEqual(provider.level.value, 35);
  assert.strictEqual(Object.isFrozen(provider.level), true);
}

assert.throws(
  () => new ConsoleProvider({ serializer: 'json' }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => new ConsoleProvider({ flatten: 1 }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
assert.throws(
  () => new ConsoleProvider({ pid: 1 }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

{
  const provider = new ConsoleProvider();
  assert.throws(
    () => { provider.level = 'unknown'; },
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}

{
  const provider = new ConsoleProvider({ serializer: () => null });
  assert.throws(
    () => provider.log({ level: { value: 30 } }),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

{
  const script = `
    const assert = require('node:assert');
    const { ConsoleProvider, create } = require('node:logger');
    const provider = new ConsoleProvider();
    const logger = create(provider);
    let synchronous = true;
    let flushed = false;

    process.on('beforeExit', () => assert.strictEqual(flushed, true));

    logger.info('asynchronous flush');
    assert.throws(
      () => provider.flushSync(),
      { code: 'ERR_INVALID_STATE' },
    );
    provider.flush((error) => {
      flushed = true;
      assert.ifError(error);
      assert.strictEqual(synchronous, false);
      assert.strictEqual(provider.stream.writing, false);
    });
    synchronous = false;
  `;
  const result = spawnSync(process.execPath, [
    '--experimental-logger',
    '--no-warnings',
    '-e',
    script,
  ], { encoding: 'utf8' });

  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  assert.strictEqual(JSON.parse(result.stdout).message, 'asynchronous flush');
}
