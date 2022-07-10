import { mustCall } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { doesNotMatch, match, strictEqual } from 'assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


// Verify no warnings are printed when no experimental features are enabled or used
spawn(execPath, [
  '--input-type=module',
  '--eval',
  `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`,
])
  .then(mustCall(({ code, signal, stderr }) => {
    doesNotMatch(
      stderr,
      /ExperimentalWarning/,
      new Error('No experimental warning(s) should be emitted when no experimental feature is enabled')
    );
    strictEqual(code, 0);
    strictEqual(signal, null);
  }));

// Verify experimental warning is printed when experimental feature is enabled
for (
  const [experiment, arg] of [
    [/Custom ESM Loaders/, `--experimental-loader=${fileURL('es-module-loaders', 'hooks-custom.mjs')}`],
    [/Network Imports/, '--experimental-network-imports'],
    [/specifier resolution/, '--experimental-specifier-resolution=node'],
  ]
) {
  spawn(execPath, [
    arg,
    '--input-type=module',
    '--eval',
    `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`,
  ])
    .then(mustCall(({ code, signal, stderr }) => {
      match(stderr, /ExperimentalWarning/);
      match(stderr, experiment);
      strictEqual(code, 0);
      strictEqual(signal, null);
    }));
}
