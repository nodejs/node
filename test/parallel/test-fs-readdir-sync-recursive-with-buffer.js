'use strict';

// Recursive readdir accepts a Buffer path.
// Refs: https://github.com/nodejs/node/issues/58892

require('../common');

const { readdirSync } = require('node:fs');
const { join } = require('node:path');

const testDirPath = join(__dirname, '..', 'common');
readdirSync(Buffer.from(testDirPath), { recursive: true });
