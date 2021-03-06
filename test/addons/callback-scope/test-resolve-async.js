'use strict';

const common = require('../../common');
const assert = require('assert');
const { testResolveAsync } = require(`./build/${common.buildType}/binding`);

// Checks that resolving promises from C++ works.

let called = false;
testResolveAsync().then(() => { called = true; });

process.on('beforeExit', () => { assert(called); });
