import { mustCall } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { match, strictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

// Verify non-js extensions fail for ESM
const child = spawn(execPath, [
  '--input-type=module',
  '--eval',
  `import ${JSON.stringify(fileURL('es-modules', 'file.unknown'))}`,
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, signal) => {
  strictEqual(code, 1);
  strictEqual(signal, null);
  match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
}));
