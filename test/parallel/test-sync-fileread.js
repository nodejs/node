'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const fixture = path.join(common.fixturesDir, 'x.txt');

assert.strictEqual(fs.readFileSync(fixture).toString(), 'xyz\n');
