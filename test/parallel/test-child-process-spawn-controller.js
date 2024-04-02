'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const fixtures = require('../common/fixtures');

const aliveScript = fixtures.path('child-process-stay-alive-forever.js');
{
  // Verify that passing an AbortSignal works
  const controller = new AbortController();
  const { signal } = controller;

  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });

  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));

  controller.abort();
}

{
  // Verify that passing an AbortSignal with custom abort error works
  const controller = new AbortController();
  const { signal } = controller;
  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });

  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
    assert.strictEqual(e.cause.name, 'Error');
    assert.strictEqual(e.cause.message, 'boom');
  }));

  controller.abort(new Error('boom'));
}

{
  const controller = new AbortController();
  const { signal } = controller;
  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });

  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
    assert.strictEqual(e.cause, 'boom');
  }));

  controller.abort('boom');
}

{
  // Verify that passing an already-aborted signal works.
  const signal = AbortSignal.abort();

  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });
  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));
}

{
  // Verify that passing an already-aborted signal with custom abort error
  // works.
  const signal = AbortSignal.abort(new Error('boom'));
  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });
  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
    assert.strictEqual(e.cause.name, 'Error');
    assert.strictEqual(e.cause.message, 'boom');
  }));
}

{
  const signal = AbortSignal.abort('boom');
  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });
  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
    assert.strictEqual(e.cause, 'boom');
  }));
}

{
  // Verify that waiting a bit and closing works
  const controller = new AbortController();
  const { signal } = controller;

  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });

  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGTERM');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));

  setTimeout(() => controller.abort(), 1);
}

{
  // Test passing a different killSignal
  const controller = new AbortController();
  const { signal } = controller;

  const cp = spawn(process.execPath, [aliveScript], {
    signal,
    killSignal: 'SIGKILL',
  });

  cp.on('exit', common.mustCall((code, killSignal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(killSignal, 'SIGKILL');
  }));

  cp.on('error', common.mustCall((e) => {
    assert.strictEqual(e.name, 'AbortError');
  }));

  setTimeout(() => controller.abort(), 1);
}

{
  // Test aborting a cp before close but after exit
  const controller = new AbortController();
  const { signal } = controller;

  const cp = spawn(process.execPath, [aliveScript], {
    signal,
  });

  cp.on('exit', common.mustCall(() => {
    controller.abort();
  }));

  cp.on('error', common.mustNotCall());

  setTimeout(() => cp.kill(), 1);
}
