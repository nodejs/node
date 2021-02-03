'use strict';
require('../common');

const path = require('path');
const assert = require('assert');

const yarnPathPackageJson = path.resolve(
  __dirname,
  '..',
  '..',
  'deps',
  'yarn',
  'package.json'
);

const pkg = require(yarnPathPackageJson);
assert(pkg.version.match(/^\d+\.\d+\.\d+$/),
       `unexpected version number: ${pkg.version}`);
