'use strict';

// Recursive readdir accepts a Buffer path.
// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');

const { readdir } = require('node:fs/promises');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', 'common');
readdir(Buffer.from(testDirPath), { recursive: true }).then(common.mustCall());
