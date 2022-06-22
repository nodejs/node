import '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';

const setupArgs = [
  '--no-warnings',
  '--input-type=module',
  '--eval',
];
const commonInput = 'import fs from "node:fs"; console.log(fs)';
const commonArgs = [
  ...setupArgs,
  commonInput,
];

{ // Verify unadulterated source is loaded when there are no loaders
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      ...setupArgs,
      'import fs from "node:fs"; console.log(typeof fs?.constants?.F_OK )',
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /number/); // node:fs is an object
  assert.strictEqual(status, 0);
}

{ // Verify loaded source is properly different when only load changes something
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /load passthru/);
  assert.match(stdout, /resolve passthru/);
  assert.match(stdout, /foo/);
  assert.strictEqual(status, 0);
}

{ // Verify multiple changes from hooks result in proper output
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-foo.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /resolve 42/); // It did go thru resolve-42
  assert.match(stdout, /foo/); // LIFO, so resolve-foo won
  assert.strictEqual(status, 0);
}

{ // Verify modifying context within resolve chain is respected
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-receiving-modified-context.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passing-modified-context.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /bar/);
  assert.strictEqual(status, 0);
}

{ // Verify multiple changes from hooks result in proper output
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-foo.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /resolve foo/); // It did go thru resolve-foo
  assert.match(stdout, /42/); // LIFO, so resolve-42 won
  assert.strictEqual(status, 0);
}

{ // Verify multiple calls to next within same loader receive correct "next" fn
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-foo.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-multiple-next-calls.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  const countFoos = stdout.match(/resolve foo/g)?.length;

  assert.strictEqual(stderr, '');
  assert.strictEqual(countFoos, 2);
  assert.strictEqual(status, 0);
}

{ // Verify next<HookName> function's `name` is correct
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /next<HookName>: nextResolve/);
  assert.strictEqual(status, 0);
}

{ // Verify error thrown for incomplete resolve chain, citing errant loader & hook
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-incomplete.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stdout, /resolve passthru/);
  assert.match(stderr, /ERR_LOADER_CHAIN_INCOMPLETE/);
  assert.match(stderr, /loader-resolve-incomplete\.mjs/);
  assert.match(stderr, /'resolve'/);
  assert.strictEqual(status, 1);
}

{ // Verify error NOT thrown when nested resolve hook signaled a short circuit
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-next-modified.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout.trim(), 'foo');
  assert.strictEqual(status, 0);
}

{ // Verify error NOT thrown when nested load hook signaled a short circuit
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-shortcircuit.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-next-modified.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(stderr, '');
  assert.match(stdout, /421/);
  assert.strictEqual(status, 0);
}

{ // Verify chain does break and throws appropriately
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-incomplete.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-foo-or-42.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.doesNotMatch(stdout, /resolve passthru/);
  assert.match(stderr, /ERR_LOADER_CHAIN_INCOMPLETE/);
  assert.match(stderr, /loader-resolve-incomplete\.mjs/);
  assert.match(stderr, /'resolve'/);
  assert.strictEqual(status, 1);
}

{ // Verify error thrown for incomplete load chain, citing errant loader & hook
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-incomplete.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stdout, /load passthru/);
  assert.match(stderr, /ERR_LOADER_CHAIN_INCOMPLETE/);
  assert.match(stderr, /loader-load-incomplete\.mjs/);
  assert.match(stderr, /'load'/);
  assert.strictEqual(status, 1);
}

{ // Verify chain does break and throws appropriately
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-incomplete.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.doesNotMatch(stdout, /load passthru/);
  assert.match(stderr, /ERR_LOADER_CHAIN_INCOMPLETE/);
  assert.match(stderr, /loader-load-incomplete\.mjs/);
  assert.match(stderr, /'load'/);
  assert.strictEqual(status, 1);
}

{ // Verify error thrown when invalid `specifier` argument passed to `nextResolve`
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-bad-next-specifier.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.strictEqual(status, 1);
  assert.match(stderr, /ERR_INVALID_ARG_TYPE/);
  assert.match(stderr, /loader-resolve-bad-next-specifier\.mjs/);
  assert.match(stderr, /'resolve' hook's nextResolve\(\) specifier/);
}

{ // Verify error thrown when invalid `context` argument passed to `nextResolve`
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-bad-next-context.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stderr, /ERR_INVALID_ARG_TYPE/);
  assert.match(stderr, /loader-resolve-bad-next-context\.mjs/);
  assert.match(stderr, /'resolve' hook's nextResolve\(\) context/);
  assert.strictEqual(status, 1);
}

{ // Verify error thrown when invalid `url` argument passed to `nextLoad`
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-bad-next-url.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stderr, /ERR_INVALID_ARG_TYPE/);
  assert.match(stderr, /loader-load-bad-next-url\.mjs/);
  assert.match(stderr, /'load' hook's nextLoad\(\) url/);
  assert.strictEqual(status, 1);
}

{ // Verify error thrown when invalid `url` argument passed to `nextLoad`
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-impersonating-next-url.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stderr, /ERR_INVALID_ARG_VALUE/);
  assert.match(stderr, /loader-load-impersonating-next-url\.mjs/);
  assert.match(stderr, /'load' hook's nextLoad\(\) url/);
  assert.strictEqual(status, 1);
}

{ // Verify error thrown when invalid `context` argument passed to `nextLoad`
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-load-bad-next-context.mjs'),
      ...commonArgs,
    ],
    { encoding: 'utf8' },
  );

  assert.match(stderr, /ERR_INVALID_ARG_TYPE/);
  assert.match(stderr, /loader-load-bad-next-context\.mjs/);
  assert.match(stderr, /'load' hook's nextLoad\(\) context/);
  assert.strictEqual(status, 1);
}
