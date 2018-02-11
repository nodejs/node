'use strict';

const common = require('../../common');
const { testResolveAsync } = require(`./build/${common.buildType}/binding`);

testResolveAsync().then(common.mustCall());
