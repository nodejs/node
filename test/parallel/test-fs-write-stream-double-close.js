'use strict';

const common = require('../common');
const fs = require('fs');

common.refreshTmpDir();

const s = fs.createWriteStream(common.tmpDir + '/rw');
s.close(common.mustCall(function() {}));
s.close(common.mustCall(function() {}));
