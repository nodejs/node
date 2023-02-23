'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('hr-time');

runner.setInitScript(`
  self.GLOBAL.isWorker = () => false;
`);
runner.pretendGlobalThisAs('Window');
runner.brandCheckGlobalScopeAttribute('performance');

runner.runJsTests();
