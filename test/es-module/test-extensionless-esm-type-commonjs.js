'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const commonjsDir = tmpdir.resolve('extensionless-esm-commonjs');
fs.mkdirSync(commonjsDir, { recursive: true });

// package.json with type: commonjs
fs.writeFileSync(
  path.join(commonjsDir, 'package.json'),
  '{\n  "type": "commonjs"\n}\n',
  'utf8'
);

// Extensionless executable with shebang + ESM syntax.
// NOTE: Execute via process.execPath to avoid PATH/env differences.
const commonjsScriptPath = path.join(commonjsDir, 'script'); // no extension
fs.writeFileSync(
  commonjsScriptPath,
  '#!/usr/bin/env node\n' +
    "console.log('script STARTED')\n" +
    "import { version } from 'node:process'\n" +
    'console.log(version)\n',
  'utf8'
);
fs.chmodSync(commonjsScriptPath, 0o755);

spawnSyncAndAssert(process.execPath, ['./script'], {
  cwd: commonjsDir,
  encoding: 'utf8',
}, {
  status: 1,
  stderr: /.+/,
  trim: true,
});

const moduleDir = tmpdir.resolve('extensionless-esm-module');
fs.mkdirSync(moduleDir, { recursive: true });

// package.json with type: module
fs.writeFileSync(
  path.join(moduleDir, 'package.json'),
  '{\n  "type": "module"\n}\n',
  'utf8'
);

const moduleScriptPath = path.join(moduleDir, 'script'); // no extension
fs.writeFileSync(
  moduleScriptPath,
  '#!/usr/bin/env node\n' +
    "console.log('script STARTED')\n" +
    "import { version } from 'node:process'\n" +
    'console.log(version)\n',
  'utf8'
);
fs.chmodSync(moduleScriptPath, 0o755);

spawnSyncAndAssert(process.execPath, ['./script'], {
  cwd: moduleDir,
  encoding: 'utf8',
}, {
  stdout: /script STARTED[\s\S]*v\d+\./,
  trim: true,
});
