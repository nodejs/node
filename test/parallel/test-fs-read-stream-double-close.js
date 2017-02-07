'use strict';

const common = require('../common');
const fs = require('fs');

const s = fs.createReadStream(__filename);

s.close(common.mustCall(noop));
s.close(common.mustCall(noop));

function noop() {}
