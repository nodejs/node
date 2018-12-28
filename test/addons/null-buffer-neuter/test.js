'use strict';

const common = require('../../common');
common.requireFlags('--expose-gc');
const binding = require(`./build/${common.buildType}/binding`);

binding.run();
