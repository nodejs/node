'use strict';

const common = require('../../common');
const assert = require('assert');
const { testResolveAsync } = require(`./build/${common.buildType}/binding`);

let called = false;
testResolveAsync().then(common.mustCall(() => {
  called = true;
}));

setTimeout(common.mustCall(() => { assert(called); }),
           common.platformTimeout(20));
