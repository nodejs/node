'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const s = fs.createWriteStream(path.join(tmpdir.path, 'rw'));

s.close(common.mustCall());
s.close(common.mustCall());
