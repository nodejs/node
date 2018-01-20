'use strict';
const common = require('../../common');
const { SetImmediate } = require(`./build/${common.buildType}/binding`);

SetImmediate(common.mustCall());
