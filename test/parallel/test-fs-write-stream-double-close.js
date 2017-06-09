'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();

const s = fs.createWriteStream(path.join(common.tmpDir, 'rw'));

s.close(common.mustCall());
s.close(common.mustCall());
