import { mustCall, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { Worker } from 'node:worker_threads';

const loaderURL = fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs');
const targetURL = fixtures.fileURL('es-modules', 'conditional-exports.mjs');

// Test that custom conditions passed to Worker via execArgv
// are forwarded to the internal ESM loader hook worker thread when using register().
{
  const worker = new Worker(
    `
    import assert from 'node:assert';
    import { register } from 'node:module';
    register(${JSON.stringify(loaderURL.href)});
    const ns = await import(${JSON.stringify(targetURL.href)});
    assert.strictEqual(ns.default, 'from custom condition');
    `,
    {
      eval: true,
      execArgv: ['--conditions', 'custom-condition', '--no-warnings'],
    }
  );

  worker.on('error', mustNotCall());
  worker.on('exit', mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}

// Test that custom conditions passed to Worker via execArgv
// are forwarded to the internal ESM loader hook worker thread when using registerHooks().
{
  const worker = new Worker(
    `
    import assert from 'node:assert';
    import { registerHooks } from 'node:module';
    registerHooks(${JSON.stringify(loaderURL.href)});
    const ns = await import(${JSON.stringify(targetURL.href)});
    assert.strictEqual(ns.default, 'from custom condition');
    `,
    {
      eval: true,
      execArgv: ['--conditions', 'custom-condition', '--no-warnings'],
    }
  );

  worker.on('error', mustNotCall());
  worker.on('exit', mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
