'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = tmpdir.resolve('extensionless-esm-commonjs');
fs.mkdirSync(dir, { recursive: true });

// package.json with type: commonjs
fs.writeFileSync(
  path.join(dir, 'package.json'),
  '{\n  "type": "commonjs"\n}\n',
  'utf8'
);

// Extensionless executable with shebang + ESM syntax.
// NOTE: Execute via process.execPath to avoid PATH/env differences.
const scriptPath = path.join(dir, 'script'); // no extension
fs.writeFileSync(
  scriptPath,
  '#!/usr/bin/env node\n' +
    "console.log('script STARTED')\n" +
    "import { version } from 'node:process'\n" +
    'console.log(version)\n',
  'utf8'
);
fs.chmodSync(scriptPath, 0o755);

spawnSyncAndAssert(process.execPath, ['./script'], {
  cwd: dir,
  encoding: 'utf8',
}, {
  status: 1,
  stderr: /.+/,
  trim: true,
});
