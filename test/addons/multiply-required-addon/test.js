'use strict';
const common = require('../../common');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);

require(bindingPath);
delete require.cache[bindingPath];
require(bindingPath);
