import { mustCall } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { doesNotMatch, match, strictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

// Verify no warnings are printed when no experimental features are enabled or used
{
  const input = `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`;
  const child = spawn(execPath, [
    '--input-type=module',
    '--eval',
    input,
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => { stderr += data; });
  child.on('close', mustCall((code, signal) => {
    strictEqual(code, 0);
    strictEqual(signal, null);
    doesNotMatch(
      stderr,
      /ExperimentalWarning/,
      new Error('No experimental warning(s) should be emitted when no experimental feature is enabled')
    );
  }));
}

// Verify experimental warning is printed when experimental feature is enabled
for (
  const [experiment, arg] of [
    [/Custom ESM Loaders/, `--experimental-loader=${fileURL('es-module-loaders', 'hooks-custom.mjs')}`],
    [/Network Imports/, '--experimental-network-imports'],
    [/specifier resolution/, '--experimental-specifier-resolution=node'],
  ]
) {
  const input = `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`;
  const child = spawn(execPath, [
    arg,
    '--input-type=module',
    '--eval',
    input,
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => { stderr += data; });
  child.on('close', mustCall((code, signal) => {
    strictEqual(code, 0);
    strictEqual(signal, null);
    match(stderr, /ExperimentalWarning/);
    match(stderr, experiment);
  }));
}
