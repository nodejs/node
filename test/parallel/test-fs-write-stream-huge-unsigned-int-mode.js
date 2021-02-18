'use strict';
require('../common');

// This test ensures that createWriteStream does not crash when the
// passed mode is a huge unsigned int32, as reported here:
// https://github.com/nodejs/node/issues/37430

const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const example = path.join(tmpdir.path, 'dummy');

fs.createWriteStream(example, { mode: 2176057344 });
