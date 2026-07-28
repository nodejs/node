'use strict';

const common = require('../../common');
const { testResolveAsync } = require(`./build/${common.buildType}/binding`);

// Checks that resolving promises from C++ works.

testResolveAsync().then(common.mustCall());
