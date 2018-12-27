'use strict';

const common = require('../../common');
if (!global.gc)
  common.requireFlags(['--expose-gc']);
const binding = require(`./build/${common.buildType}/binding`);

binding.run();
