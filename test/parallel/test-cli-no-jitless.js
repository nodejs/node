'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Interpreter-only modes are overridden to preserve Node's WebAssembly
// requirement, including alternate spellings and the runtime flag API.
const wasm = `
  const assert = require('assert');
  assert.strictEqual(typeof WebAssembly, 'object');
  const bytes = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
  new WebAssembly.Instance(new WebAssembly.Module(bytes));
`;

for (const flag of [
  '--jitless', '--lite-mode', '--lite_mode', '-jitless', '-lite_mode',
  '--no-jitless', '--no-lite-mode',
]) {
  const result = spawnSync(process.execPath, [flag, '-e', wasm], { encoding: 'utf8' });
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.stderr, '');

  const runtime = spawnSync(process.execPath, ['-e', `
    require('v8').setFlagsFromString(${JSON.stringify(flag)});
    ${wasm}
    // A new isolate must also retain WebAssembly after changing runtime flags.
    new (require('worker_threads').Worker)(${JSON.stringify(wasm)}, { eval: true });
  `], { encoding: 'utf8' });
  assert.strictEqual(runtime.status, 0, runtime.stderr);
  assert.strictEqual(runtime.stderr, '');
}

// V8 boolean flags reject explicit values, even for overridden modes.
for (const flag of ['--jitless=true', '--lite_mode=true']) {
  const result = spawnSync(process.execPath, [flag, '-e', wasm], { encoding: 'utf8' });
  assert.strictEqual(result.status, 9, result.stderr);
  assert.strictEqual(result.signal, null);
  assert.match(result.stderr, /illegal value for flag .* of type bool/);

  // The runtime API reports invalid syntax without throwing, and Node must
  // still override any flags V8 changed before reporting the error.
  const runtime = spawnSync(process.execPath, ['-e', `
    require('v8').setFlagsFromString(${JSON.stringify(flag)});
    ${wasm}
    new (require('worker_threads').Worker)(${JSON.stringify(wasm)}, { eval: true });
  `], { encoding: 'utf8' });
  assert.strictEqual(runtime.status, 0, runtime.stderr);
  assert.strictEqual(runtime.signal, null);
  assert.match(runtime.stderr, /illegal value for flag .* of type bool/);
}

// Node's overrides must not be treated as contradictory user-supplied flags.
for (const flag of ['--jitless', '--lite-mode']) {
  const result = spawnSync(process.execPath, [
    '--abort-on-contradictory-flags', flag, '-e', wasm,
  ], { encoding: 'utf8' });
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.stderr, '');
}

const options = spawnSync(process.execPath, ['--v8-options'], { encoding: 'utf8' });
assert.strictEqual(options.status, 0, options.stderr);
assert.match(options.stdout, /--jitless\b/);
assert.match(options.stdout, /--lite-mode\b/);

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
