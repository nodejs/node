'use strict';
const common = require('../common');

if (process.argv[2] === 'wasi-child-default') {
  // test default case
  const fixtures = require('../common/fixtures');
  const tmpdir = require('../common/tmpdir');
  const fs = require('fs');
  const path = require('path');

  common.expectWarning('ExperimentalWarning',
                       'WASI is an experimental feature and might change at any time');

  const { WASI } = require('wasi');
  tmpdir.refresh();
  const wasmDir = path.join(__dirname, 'wasm');
  const wasi = new WASI({
    args: ['foo', '-bar', '--baz=value'],
    env: process.env,
    preopens: {
      '/sandbox': fixtures.path('wasi'),
      '/tmp': tmpdir.path,
    },
  });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const modulePath = path.join(wasmDir, `${process.argv[3]}.wasm`);
  const buffer = fs.readFileSync(modulePath);

  (async () => {
    const { instance } = await WebAssembly.instantiate(buffer, importObject);

    wasi.start(instance);
  })().then(common.mustCall());
} else if (process.argv[2] === 'wasi-child-preview1') {
  // Test version set to preview1
  const assert = require('assert');
  const fixtures = require('../common/fixtures');
  const tmpdir = require('../common/tmpdir');
  const fs = require('fs');
  const path = require('path');

  common.expectWarning('ExperimentalWarning',
                       'WASI is an experimental feature and might change at any time');

  const { WASI } = require('wasi');
  tmpdir.refresh();
  const wasmDir = path.join(__dirname, 'wasm');
  const wasiPreview1 = new WASI({
    version: 'preview1',
    args: ['foo', '-bar', '--baz=value'],
    env: process.env,
    preopens: {
      '/sandbox': fixtures.path('wasi'),
      '/tmp': tmpdir.path,
    },
  });

  // Validate the getImportObject helper
  assert.strictEqual(wasiPreview1.wasiImport,
                     wasiPreview1.getImportObject().wasi_snapshot_preview1);
  const modulePathPreview1 = path.join(wasmDir, `${process.argv[3]}.wasm`);
  const bufferPreview1 = fs.readFileSync(modulePathPreview1);

  (async () => {
    const { instance: instancePreview1 } =
      await WebAssembly.instantiate(bufferPreview1,
                                    wasiPreview1.getImportObject());

    wasiPreview1.start(instancePreview1);
  })().then(common.mustCall());
} else {
  const assert = require('assert');
  const cp = require('child_process');
  const { checkoutEOL } = common;

  function innerRunWASI(options, args, flavor = 'default') {
    console.log('executing', options.test);
    const opts = {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'wasi',
        NODE_PLATFORM: process.platform,
      },
    };

    if (options.stdin !== undefined)
      opts.input = options.stdin;

    const child = cp.spawnSync(process.execPath, [
      ...args,
      '--experimental-wasi-unstable-preview1',
      __filename,
      'wasi-child-' + flavor,
      options.test,
    ], opts);
    console.log(child.stderr.toString());
    assert.strictEqual(child.status, options.exitCode || 0);
    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.stdout.toString(), options.stdout || '');
  }

  function runWASI(options) {
    innerRunWASI(options, ['--no-turbo-fast-api-calls']);
    innerRunWASI(options, ['--turbo-fast-api-calls']);
    innerRunWASI(options, ['--turbo-fast-api-calls'], 'preview1');
  }

  runWASI({ test: 'cant_dotdot' });

  // Tests that are currently unsupported on IBM i PASE.
  if (!common.isIBMi) {
    runWASI({ test: 'clock_getres' });
  }
  runWASI({ test: 'exitcode', exitCode: 120 });
  runWASI({ test: 'fd_prestat_get_refresh' });
  runWASI({ test: 'freopen', stdout: `hello from input2.txt${checkoutEOL}` });
  runWASI({ test: 'ftruncate' });
  runWASI({ test: 'getentropy' });

  // Tests that are currently unsupported on IBM i PASE.
  if (!common.isIBMi) {
    runWASI({ test: 'getrusage' });
  }
  runWASI({ test: 'gettimeofday' });
  runWASI({ test: 'main_args' });
  runWASI({ test: 'notdir' });
  runWASI({ test: 'poll' });
  runWASI({ test: 'preopen_populates' });

  if (!common.isWindows && process.platform !== 'android') {
    runWASI({ test: 'readdir' });
  }

  runWASI({ test: 'read_file', stdout: `hello from input.txt${checkoutEOL}` });
  runWASI({
    test: 'read_file_twice',
    stdout: `hello from input.txt${checkoutEOL}hello from input.txt${checkoutEOL}`,
  });
  runWASI({ test: 'stat' });
  runWASI({ test: 'sock' });
  runWASI({ test: 'write_file' });

  // Tests that are currently unsupported on Windows.
  if (!common.isWindows) {
    runWASI({ test: 'stdin', stdin: 'hello world', stdout: 'hello world' });
  }
}
