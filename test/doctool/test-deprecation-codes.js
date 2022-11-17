'use strict';

require('../common');
const path = require('path');
const { spawn } = require('child_process');

const script = path.join(
  __dirname,
  '..',
  '..',
  'tools',
  'doc',
  'deprecationCodes.mjs',
);

const mdPath = path.join(
  __dirname,
  '..',
  '..',
  'doc',
  'api',
  'deprecations.md',
);

const cp = spawn(process.execPath, [script, mdPath], { encoding: 'utf-8', stdio: 'inherit' });

cp.on('error', (err) => { throw err; });
cp.on('exit', (code) => process.exit(code));
