'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const fsPromises = require('fs/promises');

assert.strictEqual(fsPromises, fs.promises);
assert.strictEqual(fsPromises.constants, fs.constants);
