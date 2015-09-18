'use strict';

const common = require('../common');
const fs = require('fs');

common.refreshTmpDir();
fs.writeFileSync(common.tmpDir + '/ro', '');

const s = fs.createReadStream(common.tmpDir + '/ro');
s.close(common.mustCall(function() {}));
s.close(common.mustCall(function() {}));
