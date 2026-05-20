'use strict';
require('../common');

const path = require('path');
const assert = require('assert');

const npmPathPackageJson = path.resolve(
  __dirname,
  '..',
  '..',
  'deps',
  'npm',
  'package.json'
);

const pkg = require(npmPathPackageJson);
assert(pkg.version.match(/^\d+\.\d+\.\d+$/),
       `unexpected version number: ${pkg.version}`);
