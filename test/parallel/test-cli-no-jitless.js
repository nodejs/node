'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Interpreter-only modes must not bypass Node's WebAssembly requirement,
// including through V8's alternate flag spellings and runtime flag API.
for (const flag of [
  '--jitless', '--lite-mode', '--lite_mode', '-jitless', '-lite_mode',
  '--jitless=true', '--lite_mode=true', '--no-jitless', '--no-lite-mode',
]) {
  const result = spawnSync(process.execPath, [flag, '-e', ''], { encoding: 'utf8' });
  assert.strictEqual(result.status, 9, result.stderr);
  assert.strictEqual(result.signal, null);
  assert.match(result.stderr, /bad option:/);

  const runtime = spawnSync(process.execPath, ['-e', `
    require('v8').setFlagsFromString(${JSON.stringify(flag)});
    const bytes = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
    new WebAssembly.Instance(new WebAssembly.Module(bytes));
  `], { encoding: 'utf8' });
  assert.strictEqual(runtime.status, 0, runtime.stderr);
  assert.match(runtime.stderr, /unrecognized flag/);
}

const options = spawnSync(process.execPath, ['--v8-options'], { encoding: 'utf8' });
assert.strictEqual(options.status, 0, options.stderr);
assert.doesNotMatch(options.stdout, /--(?:jitless|lite[-_]mode)\b/);

for (const flag of ['--jitless', '--lite-mode', '--lite_mode']) {
  assert.strictEqual(process.allowedNodeEnvironmentFlags.has(flag), false);
  if (!process.config.variables.node_without_node_options) {
    const result = spawnSync(process.execPath, ['-e', ''], {
      encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: flag },
    });
    assert.strictEqual(result.status, 9, result.stderr);
    assert.match(result.stderr, /is not allowed in NODE_OPTIONS/);
  }
}

// Workers must inherit a runtime with working WebAssembly as well.
const { Worker } = require('worker_threads');
const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  const bytes = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
  new WebAssembly.Instance(new WebAssembly.Module(bytes));
  parentPort.postMessage(typeof WebAssembly);
`, { eval: true });
worker.on('message', common.mustCall((value) => assert.strictEqual(value, 'object')));
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
