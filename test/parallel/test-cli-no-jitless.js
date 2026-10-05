'use strict';

const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

// Interpreter-only modes are rejected before V8 can apply them, including
// alternate spellings and the runtime flag API.
const wasm = `
  const assert = require('assert');
  assert.strictEqual(typeof WebAssembly, 'object');
  const bytes = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
  new WebAssembly.Instance(new WebAssembly.Module(bytes));
`;

for (const flag of [
  '--jitless', '--lite-mode', '--lite_mode', '-jitless', '-lite-mode', '-lite_mode',
  '--jitless=true', '--lite_mode=true',
]) {
  // Contradiction checking must not turn Node's rejection into a V8 abort.
  for (const prefix of [[], ['--abort-on-contradictory-flags']]) {
    const result = spawnSync(process.execPath, [...prefix, flag, '-e', wasm], { encoding: 'utf8' });
    assert.strictEqual(result.status, 9, result.stderr);
    assert.strictEqual(result.signal, null);
    assert.strictEqual(result.stdout, '');
    assert(result.stderr.includes(`Node.js does not support V8 flag ${flag}.`));
  }

  for (const flags of [flag, `--expose-gc\t\n${flag} --no-jitless`]) {
    const runtime = spawnSync(process.execPath, ['-e', `
      const assert = require('node:assert');
      assert.throws(() => {
        require('node:v8').setFlagsFromString(${JSON.stringify(flags)});
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: ${JSON.stringify(`Node.js does not support V8 flag ${flag}.`)}
      });
      // Catching the error must leave this isolate and new workers usable.
      assert.strictEqual(globalThis.gc, undefined);
      {
        ${wasm}
      }
      new (require('node:worker_threads').Worker)(${JSON.stringify(wasm)}, { eval: true });
    `], { encoding: 'utf8' });
    assert.strictEqual(runtime.status, 0, runtime.stderr);
    assert.strictEqual(runtime.signal, null);
    assert.strictEqual(runtime.stdout, '');
    assert.strictEqual(runtime.stderr, '');
  }
}

// A worker can catch the rejection without changing the global V8 flags.
for (const flag of ['--jitless', '--lite-mode']) {
  const source = `
    const assert = require('node:assert');
    assert.throws(() => {
      require('node:v8').setFlagsFromString(${JSON.stringify(flag)});
    }, { code: 'ERR_INVALID_ARG_VALUE' });
    {
      ${wasm}
    }
  `;
  const runtime = spawnSync(process.execPath, ['-e', `
    const assert = require('node:assert');
    const worker = new (require('node:worker_threads').Worker)(${JSON.stringify(source)}, { eval: true });
    worker.on('exit', (code) => {
      assert.strictEqual(code, 0);
      {
        ${wasm}
      }
      new (require('node:worker_threads').Worker)(${JSON.stringify(wasm)}, { eval: true });
    });
  `], { encoding: 'utf8' });
  assert.strictEqual(runtime.status, 0, runtime.stderr);
  assert.strictEqual(runtime.signal, null);
  assert.strictEqual(runtime.stderr, '');
}

// Disabling these modes remains valid and preserves working WebAssembly.
for (const flag of ['--no-jitless', '--nojitless', '--no-lite-mode', '--no_lite_mode', '--nolite_mode']) {
  const result = spawnSync(process.execPath, [flag, '-e', wasm], { encoding: 'utf8' });
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.stderr, '');

  const runtime = spawnSync(process.execPath, ['-e', `
    require('node:v8').setFlagsFromString(${JSON.stringify(flag)});
    ${wasm}
    new (require('node:worker_threads').Worker)(${JSON.stringify(wasm)}, { eval: true });
  `], { encoding: 'utf8' });
  assert.strictEqual(runtime.status, 0, runtime.stderr);
  assert.strictEqual(runtime.signal, null);
  assert.strictEqual(runtime.stderr, '');
}

// Application arguments and text after a NUL are not V8 flags.
const argument = spawnSync(process.execPath, ['-e', wasm, '--', '--jitless'], { encoding: 'utf8' });
assert.strictEqual(argument.status, 0, argument.stderr);
assert.strictEqual(argument.stderr, '');
const nul = spawnSync(process.execPath, ['-e', `
  require('node:v8').setFlagsFromString('--no-jitless\\0 --jitless');
  ${wasm}
`], { encoding: 'utf8' });
assert.strictEqual(nul.status, 0, nul.stderr);
assert.strictEqual(nul.stderr, '');

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
