import { mustCall } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { match, strictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

// Verify experimental warning is printed
const child = spawn(execPath, [
  '--experimental-specifier-resolution=node',
  '--input-type=module',
  '--eval',
  `import ${JSON.stringify(fileURL('es-module-specifiers', 'package-type-module'))}`,
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, signal) => {
  strictEqual(code, 0);
  strictEqual(signal, null);
  match(stderr, /ExperimentalWarning: The Node\.js specifier resolution flag is experimental/);
}));
