'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('workers');

runner.pretendGlobalThisAs('Window');
runner.runJsTests();
