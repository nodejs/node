'use strict';
require('../common');

const path = require('path');
const assert = require('assert');

const corepackPathPackageJson = path.resolve(
  __dirname,
  '..',
  '..',
  'deps',
  'corepack',
  'package.json'
);

const pkg = require(corepackPathPackageJson);
assert(pkg.version.match(/^\d+\.\d+\.\d+$/),
       `unexpected version number: ${pkg.version}`);
