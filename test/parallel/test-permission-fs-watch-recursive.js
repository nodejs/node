'use strict';

// Under the permission model, a recursive fs.watch() on a directory granted
// with an exact (non-wildcard) read scope must not leak the names of nested
// descendants the process is not allowed to read.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('no crypto');

// Recursive fs.watch() is backed by native OS facilities only on macOS and
// Windows; elsewhere it uses a JS fallback that goes through the regular,
// already permission-checked fs APIs.
if (!common.isMacOS && !common.isWindows)
  common.skip('recursive fs.watch is only native on macOS and Windows');

const assert = require('assert');
const path = require('path');
const { spawn } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// `watched` does not exist yet, so the grant stays exact (no wildcard is added
// for a non-existent directory) and its future children are not readable.
const watched = tmpdir.resolve('watched');

const child = spawn(process.execPath, [
  '--permission',
  `--allow-fs-read=${watched}`,
  `--allow-fs-write=${watched}`,
  '-e',
  `
    const fs = require('fs');
    const path = require('path');
    const root = ${JSON.stringify(watched)};
    fs.mkdirSync(root);
    const names = [];
    const w = fs.watch(root, { recursive: true }, (event, filename) => {
      if (filename !== null && filename !== undefined) names.push(filename);
    });
    process.stdout.write('READY\\n');
    setTimeout(() => { w.close(); process.stdout.write('NAMES:' + JSON.stringify(names) + '\\n'); }, 1500);
  `,
], { encoding: 'utf8' });

let stdout = '';
let created = false;
child.stdout.setEncoding('utf8');
child.stdout.on('data', (chunk) => {
  stdout += chunk;
  if (!created && stdout.includes('READY\n')) {
    created = true;
    // As an unrestricted parent, create a nested descendant the child is not
    // allowed to read (child can only create `watched` itself, not children).
    const fs = require('fs');
    fs.mkdirSync(path.join(watched, 'sub'));
    fs.writeFileSync(path.join(watched, 'sub', 'secret.txt'), 'secret');
  }
});

child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0, `child exited with ${code}\n${stdout}`);
  const match = stdout.match(/NAMES:(.*)/);
  assert(match, `missing NAMES output:\n${stdout}`);
  const names = JSON.parse(match[1]);

  // The nested, unreadable descendant name must not have been delivered.
  const leaked = names.some((n) => n.replaceAll(path.sep, '/').includes('sub/secret.txt'));
  assert.strictEqual(leaked, false,
                     `recursive watch leaked a denied descendant: ${JSON.stringify(names)}`);
}));
