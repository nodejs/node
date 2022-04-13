'use strict';

require('../common');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const s = fs.createWriteStream(path.join(tmpdir.path, 'nocallback'));

s.end('hello world');
s.close();
