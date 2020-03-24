'use strict';
const common = require('../common');

if (process.argv[2] === 'wasi-child') {
  const fixtures = require('../common/fixtures');
  const tmpdir = require('../common/tmpdir');
  const fs = require('fs');
  const path = require('path');

  common.expectWarning('ExperimentalWarning',
                       'WASI is an experimental feature. This feature could ' +
                       'change at any time');

  const { WASI } = require('wasi');
  tmpdir.refresh();
  const wasmDir = path.join(__dirname, 'wasm');
  const wasi = new WASI({
    args: ['foo', '-bar', '--baz=value'],
    env: process.env,
    preopens: {
      '/sandbox': fixtures.path('wasi'),
      '/tmp': tmpdir.path
    }
  });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const modulePath = path.join(wasmDir, `${process.argv[3]}.wasm`);
  const buffer = fs.readFileSync(modulePath);

  (async () => {
    const { instance } = await WebAssembly.instantiate(buffer, importObject);

    wasi.start(instance);
  })();
} else {
  const assert = require('assert');
  const cp = require('child_process');
  const { EOL } = require('os');

  function runWASI(options) {
    console.log('executing', options.test);
    const opts = { env: { ...process.env, NODE_DEBUG_NATIVE: 'wasi' } };

    if (options.stdin !== undefined)
      opts.input = options.stdin;

    const child = cp.spawnSync(process.execPath, [
      '--experimental-wasi-unstable-preview1',
      '--experimental-wasm-bigint',
      __filename,
      'wasi-child',
      options.test
    ], opts);
    console.log(child.stderr.toString());
    assert.strictEqual(child.status, options.exitCode || 0);
    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.stdout.toString(), options.stdout || '');
  }

  runWASI({ test: 'cant_dotdot' });

  // Tests that are currently unsupported on IBM i PASE.
  if (!common.isIBMi) {
    runWASI({ test: 'clock_getres' });
  }
  runWASI({ test: 'exitcode', exitCode: 120 });
  runWASI({ test: 'fd_prestat_get_refresh' });
  runWASI({ test: 'freopen', stdout: `hello from input2.txt${EOL}` });
  runWASI({ test: 'getentropy' });

  // Tests that are currently unsupported on IBM i PASE.
  if (!common.isIBMi) {
    runWASI({ test: 'getrusage' });
  }
  runWASI({ test: 'gettimeofday' });
  runWASI({ test: 'link' });
  runWASI({ test: 'main_args' });
  runWASI({ test: 'notdir' });
  // runWASI({ test: 'poll' });
  runWASI({ test: 'preopen_populates' });
  runWASI({ test: 'read_file', stdout: `hello from input.txt${EOL}` });
  runWASI({
    test: 'read_file_twice',
    stdout: `hello from input.txt${EOL}hello from input.txt${EOL}`
  });
  runWASI({ test: 'stat' });
  runWASI({ test: 'write_file' });

  // Tests that are currently unsupported on Windows.
  if (!common.isWindows) {
    runWASI({ test: 'stdin', stdin: 'hello world', stdout: 'hello world' });
  }
}
