'use strict';

require('../common');
const path = require('path');
const { execFileSync } = require('child_process');

const script = path.join(
  __dirname,
  '..',
  '..',
  'tools',
  'doc',
  'deprecationCodes.js'
);

const mdPath = path.join(
  __dirname,
  '..',
  '..',
  'doc',
  'api',
  'deprecations.md'
);

execFileSync(process.execPath, [script, mdPath], { encoding: 'utf-8' });
