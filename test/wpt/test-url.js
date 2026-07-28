'use strict';

const { skip } = require('../common');

if (process.config.variables.node_shared_ada) {
  skip('Different versions of Ada affect the WPT tests');
}

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

runner.pretendGlobalThisAs('Window');
runner.runJsTests();
