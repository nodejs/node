'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const filename = path.join(tmpdir.path, 'nonExistingModule.js');
assert.throws(() => {
  require(filename);
}, /Cannot find module/);

fs.writeFileSync(filename, 'module.exports = \'Hello Node!\';');

assert.strictEqual(require(filename), 'Hello Node!');
