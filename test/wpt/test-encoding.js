'use strict';
require('../common');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

runner.runJsTests();
