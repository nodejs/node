'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('hr-time');

runner.pretendGlobalThisAs('Window');
runner.brandCheckGlobalScopeAttribute('performance');

runner.runJsTests();
