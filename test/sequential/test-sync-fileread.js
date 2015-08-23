'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const fixture = path.join(__dirname, '../fixtures/x.txt');

assert.equal('xyz\n', fs.readFileSync(fixture));
