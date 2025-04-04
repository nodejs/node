'use strict';

// eslint-disable-next-line no-unused-vars
const common = require('../common');
const assert = require('assert');
const { join } = require('path');
const { Worker, makeSync } = require('worker_threads');

// Test makeSync and wire functionality
{
  const buffer = new SharedArrayBuffer(1024, {
    maxByteLength: 64 * 1024 * 1024,
  });
  const worker = new Worker(join(__dirname, '..', 'fixtures', 'everysync', 'echo.mjs'), {
    workerData: {
      data: buffer,
    },
  });

  const api = makeSync(buffer);

  assert.strictEqual(api.echo(42), 42);
  assert.strictEqual(api.echo('test'), 'test');
  assert.deepStrictEqual(api.echo({ foo: 'bar' }), { foo: 'bar' });

  worker.terminate();
}

// Test timeout failure
{
  const buffer = new SharedArrayBuffer(1024, {
    maxByteLength: 64 * 1024 * 1024,
  });
  const worker = new Worker(join(__dirname, '..', 'fixtures', 'everysync', 'failure.mjs'), {
    workerData: {
      data: buffer,
    },
  });

  const api = makeSync(buffer, { timeout: 100 });

  assert.throws(() => api.fail(), {
    code: 'ERR_WORKER_MESSAGING_TIMEOUT'
  });

  worker.terminate();
}

// Test initialization timeout
{
  const buffer = new SharedArrayBuffer(1024, {
    maxByteLength: 64 * 1024 * 1024,
  });

  assert.throws(() => makeSync(buffer, { timeout: 100 }), {
    code: 'ERR_WORKER_MESSAGING_TIMEOUT'
  });
}
