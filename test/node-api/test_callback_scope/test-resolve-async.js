'use strict';
// Addons: binding, binding_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const { testResolveAsync } = require(addonPath);

testResolveAsync().then(common.mustCall());
