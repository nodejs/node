'use strict';
const common = require('../common');
const fs = require('fs');

const file = fs.createReadStream(__filename);

file.on('ready', common.mustCall( () => {}, 1));
