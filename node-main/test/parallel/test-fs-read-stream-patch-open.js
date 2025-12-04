'use strict';
const common = require('../common');
const fs = require('fs');

fs.ReadStream.prototype.open = common.mustCall();
fs.createReadStream('asd');
