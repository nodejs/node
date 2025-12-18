'use strict';

require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const s = fs.createWriteStream(tmpdir.resolve('nocallback'));

s.end('hello world');
s.close();
